#pragma once

#include "BKE_image.h"
#include "IMB_imbuf_types.h"

#include "FN_cpp_type.h"

#include "particle_function.hpp"
#include "action_contexts.hpp"

namespace BParticles {

using FN::CPPType;

class SurfaceNormalInputProvider : public ParticleFunctionInputProvider {
  Optional<ParticleFunctionInputArray> get(InputProviderInterface &interface) override;
};

class SurfaceVelocityInputProvider : public ParticleFunctionInputProvider {
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

}  // namespace BParticles
