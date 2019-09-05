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

 public:
  SurfaceImageInputProvider(Image *image);
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

}  // namespace BParticles
