#pragma once

#include "BKE_image.h"
#include "IMB_imbuf_types.h"

#include "particle_function.hpp"

namespace BParticles {

class AttributeInputProvider : public ParticleFunctionInputProvider {
 private:
  std::string m_name;

 public:
  AttributeInputProvider(StringRef name) : m_name(name)
  {
  }

  ParticleFunctionInputArray get(InputProviderInterface &interface) override;
};

class CollisionNormalInputProvider : public ParticleFunctionInputProvider {
  ParticleFunctionInputArray get(InputProviderInterface &interface) override;
};

class AgeInputProvider : public ParticleFunctionInputProvider {
  ParticleFunctionInputArray get(InputProviderInterface &interface) override;
};

class SurfaceImageInputProvider : public ParticleFunctionInputProvider {
 private:
  Image *m_image;
  ImageUser m_image_user;
  ImBuf *m_ibuf;

 public:
  SurfaceImageInputProvider(Image *image);
  ~SurfaceImageInputProvider();

  ParticleFunctionInputArray get(InputProviderInterface &interface) override;
};

}  // namespace BParticles
