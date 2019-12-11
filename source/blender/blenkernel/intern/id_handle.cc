#include "BLI_utildefines.h"
#include "BLI_hash.h"

#include "BKE_id_handle.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_image_types.h"

namespace BKE {

IDHandle::IDHandle(ID *id)
{
  BLI_assert(id != nullptr);
  m_identifier = BLI_hash_string(id->name);
}

ObjectIDHandle::ObjectIDHandle(Object *object) : IDHandle(&object->id)
{
}

ImageIDHandle::ImageIDHandle(Image *image) : IDHandle(&image->id)
{
}

static IDHandleLookup empty_id_handle_lookup;

const IDHandleLookup &IDHandleLookup::Empty()
{
  return empty_id_handle_lookup;
}

}  // namespace BKE
