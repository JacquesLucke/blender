#pragma once

#include "BKE_image.h"
#include "IMB_imbuf_types.h"

#include "particle_function.hpp"
#include "action_contexts.hpp"

namespace BParticles {

class AttributeInputProvider : public ParticleFunctionInputProvider {
 private:
  AttributeType m_type;
  std::string m_name;

 public:
  AttributeInputProvider(AttributeType type, StringRef name) : m_type(type), m_name(name)
  {
  }

  Optional<ParticleFunctionInputArray> get(InputProviderInterface &interface) override;
};

class SurfaceNormalInputProvider : public ParticleFunctionInputProvider {
  Optional<ParticleFunctionInputArray> get(InputProviderInterface &interface) override;
};

class SurfaceVelocityInputProvider : public ParticleFunctionInputProvider {
  Optional<ParticleFunctionInputArray> get(InputProviderInterface &interface) override;
};

class AgeInputProvider : public ParticleFunctionInputProvider {
  Optional<ParticleFunctionInputArray> get(InputProviderInterface &interface) override;
};

class SurfaceImageInputProvider : public ParticleFunctionInputProvider {
 private:
  Image *m_image;
  ImageUser m_image_user;
  ImBuf *m_ibuf;
  Optional<std::string> m_uv_map_name;

 public:
  SurfaceImageInputProvider(Image *image, Optional<std::string> uv_map_name);
  ~SurfaceImageInputProvider();

  Optional<ParticleFunctionInputArray> get(InputProviderInterface &interface) override;

 private:
  Optional<ParticleFunctionInputArray> compute_colors(InputProviderInterface &interface,
                                                      MeshSurfaceContext *surface_info,
                                                      ArrayRef<uint> surface_info_mapping);
};

class VertexWeightInputProvider : public ParticleFunctionInputProvider {
 private:
  std::string m_group_name;

 public:
  VertexWeightInputProvider(StringRef group_name) : m_group_name(group_name)
  {
  }

  Optional<ParticleFunctionInputArray> get(InputProviderInterface &interface) override;

 private:
  Optional<ParticleFunctionInputArray> compute_weights(InputProviderInterface &interface,
                                                       MeshSurfaceContext *surface_info,
                                                       ArrayRef<uint> surface_info_mapping);
};

class RandomFloatInputProvider : public ParticleFunctionInputProvider {
 private:
  uint m_seed;

 public:
  RandomFloatInputProvider(uint seed) : m_seed(seed)
  {
  }

  Optional<ParticleFunctionInputArray> get(InputProviderInterface &interface) override;
};

}  // namespace BParticles
