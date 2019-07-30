#include "BLI_timeit.hpp"

#include "particles_container.hpp"

#define CACHED_BLOCKS_PER_CONTAINER 5

namespace BParticles {

ParticlesBlock::ParticlesBlock(ParticlesContainer &container, AttributeArraysCore &attributes_core)
    : m_container(container), m_attributes_core(attributes_core)
{
}

ParticlesContainer::ParticlesContainer(AttributesInfo attributes, uint block_size)
    : m_attributes_info(std::move(attributes)), m_block_size(block_size), m_next_particle_id(0)
{
}

ParticlesContainer::~ParticlesContainer()
{
  for (ParticlesBlock *block : m_active_blocks) {
    this->free_block(block);
  }
  for (ParticlesBlock *block : m_cached_blocks) {
    this->free_block(block);
  }
}

ParticlesBlock &ParticlesContainer::new_block()
{
  SCOPED_TIMER(__func__);

  std::lock_guard<std::mutex> lock(m_blocks_mutex);

  if (!m_cached_blocks.empty()) {
    ParticlesBlock *block = m_cached_blocks.pop();
    m_active_blocks.add_new(block);
    return *block;
  }

  ParticlesBlock *block = this->allocate_block();
  m_active_blocks.add_new(block);
  return *block;
}

void ParticlesContainer::release_block(ParticlesBlock &block)
{
  std::lock_guard<std::mutex> lock(m_blocks_mutex);

  BLI_assert(block.active_amount() == 0);
  BLI_assert(m_active_blocks.contains(&block));
  BLI_assert(&block.container() == this);

  m_active_blocks.remove(&block);
  if (m_cached_blocks.size() < CACHED_BLOCKS_PER_CONTAINER) {
    m_cached_blocks.push(&block);
  }
  else {
    this->free_block(&block);
  }
}

ParticlesBlock *ParticlesContainer::allocate_block()
{
  AttributeArraysCore attributes_core = AttributeArraysCore::NewWithSeparateAllocations(
      m_attributes_info, m_block_size);
  ParticlesBlock *block = new ParticlesBlock(*this, attributes_core);
  return block;
}

void ParticlesContainer::free_block(ParticlesBlock *block)
{
  block->attributes_core().free_buffers();
  delete block;
}

static Vector<int> map_attribute_indices(AttributesInfo &from_info, AttributesInfo &to_info)
{
  Vector<int> mapping;
  mapping.reserve(from_info.size());

  for (uint from_index : from_info.attribute_indices()) {
    StringRef name = from_info.name_of(from_index);
    int to_index = to_info.attribute_index_try(name);
    if (to_index == -1) {
      mapping.append(-1);
    }
    else if (from_info.type_of(from_index) != to_info.type_of(to_index)) {
      mapping.append(-1);
    }
    else {
      mapping.append((uint)to_index);
    }
  }

  return mapping;
}

void ParticlesContainer::update_attributes(AttributesInfo new_info)
{

  AttributesInfo &old_info = m_attributes_info;

  Vector<int> new_to_old_mapping = map_attribute_indices(new_info, old_info);
  Vector<int> old_to_new_mapping = map_attribute_indices(old_info, new_info);

  Vector<uint> unused_old_indices;
  for (uint i = 0; i < old_to_new_mapping.size(); i++) {
    if (old_to_new_mapping[i] == -1) {
      unused_old_indices.append(i);
    }
  }

  Vector<uint> indices_to_allocate;
  for (uint i = 0; i < new_to_old_mapping.size(); i++) {
    if (new_to_old_mapping[i] == -1) {
      indices_to_allocate.append(i);
    }
  }

  m_attributes_info = new_info;

  Vector<void *> arrays;
  arrays.reserve(new_info.size());

  Vector<ParticlesBlock *> all_blocks;
  all_blocks.extend(m_active_blocks);
  all_blocks.extend(m_cached_blocks);
  for (ParticlesBlock *block : all_blocks) {
    arrays.clear();

    for (uint new_index : new_info.attribute_indices()) {
      int old_index = new_to_old_mapping[new_index];
      AttributeType type = new_info.type_of(new_index);

      if (old_index == -1) {
        arrays.append(MEM_malloc_arrayN(m_block_size, size_of_attribute_type(type), __func__));
      }
      else {
        arrays.append(block->attributes_core().get_ptr((uint)old_index));
      }
    }

    for (uint old_index : unused_old_indices) {
      void *ptr = block->attributes_core().get_ptr(old_index);
      MEM_freeN(ptr);
    }

    block->m_attributes_core = AttributeArraysCore(m_attributes_info, arrays, m_block_size);

    for (uint new_index : indices_to_allocate) {
      block->m_attributes_core.slice_all().init_default(new_index);
    }
  }
}

void ParticlesContainer::flatten_attribute_data(StringRef attribute_name, void *dst)
{
  uint attribute_index = m_attributes_info.attribute_index(attribute_name);
  uint element_size = size_of_attribute_type(m_attributes_info.type_of(attribute_index));

  uint offset = 0;
  for (ParticlesBlock *block : m_active_blocks) {
    uint amount = block->active_amount();
    void *src = block->attributes().get_ptr(attribute_index);
    memcpy(POINTER_OFFSET(dst, offset), src, amount * element_size);
    offset += amount * element_size;
  }
}

void ParticlesBlock::MoveUntilFull(ParticlesBlock &from, ParticlesBlock &to)
{
  BLI_assert(&from.container() == &to.container());
  uint move_amount = MIN2(from.active_amount(), to.unused_amount());

  if (move_amount == 0) {
    return;
  }

  uint src_start = from.active_amount() - move_amount;
  uint dst_start = to.first_unused_index();

  uint attribute_amount = from.container().attributes_info().size();
  for (uint i = 0; i < attribute_amount; i++) {
    void *from_buffer = from.attributes_core().get_ptr(i);
    void *to_buffer = to.attributes_core().get_ptr(i);
    AttributeType type = from.attributes_core().get_type(i);
    uint size = size_of_attribute_type(type);
    memcpy((char *)to_buffer + size * dst_start,
           (char *)from_buffer + size * src_start,
           size * move_amount);
  }

  from.active_amount() -= move_amount;
  to.active_amount() += move_amount;
}

void ParticlesBlock::Compress(ArrayRef<ParticlesBlock *> blocks)
{
  std::sort(blocks.begin(), blocks.end(), [](ParticlesBlock *a, ParticlesBlock *b) {
    return a->active_amount() < b->active_amount();
  });

  uint last_non_full = blocks.size() - 1;

  for (uint i = 0; i < blocks.size(); i++) {
    while (i < last_non_full) {
      ParticlesBlock &block = *blocks[last_non_full];
      if (block.is_full()) {
        last_non_full--;
        continue;
      }
      ParticlesBlock::MoveUntilFull(*blocks[i], block);
      if (blocks[i]->active_amount() == 0) {
        break;
      }
    }
  }
}

void ParticlesBlock::move(uint old_index, uint new_index)
{
  AttributeArrays attributes = this->attributes_all();

  for (uint attribute_index : attributes.info().attribute_indices()) {
    void *ptr = attributes.get_ptr(attribute_index);
    uint size = attributes.attribute_stride(attribute_index);
    void *src = POINTER_OFFSET(ptr, old_index * size);
    void *dst = POINTER_OFFSET(ptr, new_index * size);
    memcpy(dst, src, size);
  }
}

}  // namespace BParticles
