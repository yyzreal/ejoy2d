
/*
	This particle system is come from cocos2d-x ( https://github.com/cocos2d/cocos2d-x )
	The origin source is :
		https://github.com/cocos2d/cocos2d-x/blob/develop/cocos/2d/CCParticleSystem.cpp
	Need rewrite later :)
 */



#include "particle.h"
#include "spritepack.h"

#include <math.h>

static inline float
clampf(float x) {
	if (x < 0)
		return 0;
	if (x > 1.0f)
		return 1.0f;
	return x;
}

inline static float
RANDOM_M11(unsigned int *seed) {
	*seed = (*seed * 134775813 + 1) % 0x7fff;
	union {
		uint32_t d;
		float f;
	} u;
	u.d = (((uint32_t)rand() & 0x7fff) << 8) | 0x40000000;
	return u.f - 3.0f;
}

static void
_initParticle(struct particle_system *ps, struct particle* particle) {
	uint32_t RANDSEED = rand();

	particle->timeToLive = ps->life + ps->lifeVar * RANDOM_M11(&RANDSEED);
	if (particle->timeToLive <= 0) {
		return;
	}

	particle->startPos = ps->sourcePosition;
	particle->pos.x = ps->posVar.x * RANDOM_M11(&RANDSEED);
	particle->pos.y = ps->posVar.y * RANDOM_M11(&RANDSEED);

	struct color4f *start = &particle->color;
	start->r = clampf(ps->startColor.r + ps->startColorVar.r * RANDOM_M11(&RANDSEED));
	start->g = clampf(ps->startColor.g + ps->startColorVar.g * RANDOM_M11(&RANDSEED));
	start->b = clampf(ps->startColor.b + ps->startColorVar.b * RANDOM_M11(&RANDSEED));
	start->a = clampf(ps->startColor.a + ps->startColorVar.a * RANDOM_M11(&RANDSEED));

	struct color4f end;
	end.r = clampf(ps->endColor.r + ps->endColorVar.r * RANDOM_M11(&RANDSEED));
	end.g = clampf(ps->endColor.g + ps->endColorVar.g * RANDOM_M11(&RANDSEED));
	end.b = clampf(ps->endColor.b + ps->endColorVar.b * RANDOM_M11(&RANDSEED));
	end.a = clampf(ps->endColor.a + ps->endColorVar.a * RANDOM_M11(&RANDSEED));

	particle->deltaColor.r = (end.r - start->r) / particle->timeToLive;
	particle->deltaColor.g = (end.g - start->g) / particle->timeToLive;
	particle->deltaColor.b = (end.b - start->b) / particle->timeToLive;
	particle->deltaColor.a = (end.a - start->a) / particle->timeToLive;

	float startS = ps->startSize + ps->startSizeVar * RANDOM_M11(&RANDSEED);
	if (startS < 0) {
		startS = 0;
	}

	particle->size = startS;

	if (ps->endSize == START_SIZE_EQUAL_TO_END_SIZE) {
		particle->deltaSize = 0;
	} else {
		float endS = ps->endSize + ps->endSizeVar * RANDOM_M11(&RANDSEED);
		if (endS < 0) {
			endS = 0;
		}
		particle->deltaSize = (endS - startS) / particle->timeToLive;
	}


	// rotation
	float startA = ps->startSpin + ps->startSpinVar * RANDOM_M11(&RANDSEED);
	float endA = ps->endSpin + ps->endSpinVar * RANDOM_M11(&RANDSEED);
	particle->rotation = startA;
	particle->deltaRotation = (endA - startA) / particle->timeToLive;

	// direction
	float a = CC_DEGREES_TO_RADIANS( ps->angle + ps->angleVar * RANDOM_M11(&RANDSEED) );

	// Mode Gravity: A
	if (ps->emitterMode == PARTICLE_MODE_GRAVITY) {
		struct point v;
		v.x = cosf(a);
		v.y = sinf(a);
		float s = ps->mode.A.speed + ps->mode.A.speedVar * RANDOM_M11(&RANDSEED);

		// direction
		particle->mode.A.dir.x = v.x * s ;
		particle->mode.A.dir.y = v.y * s ;

		// radial accel
		particle->mode.A.radialAccel = ps->mode.A.radialAccel + ps->mode.A.radialAccelVar * RANDOM_M11(&RANDSEED);


		// tangential accel
		particle->mode.A.tangentialAccel = ps->mode.A.tangentialAccel + ps->mode.A.tangentialAccelVar * RANDOM_M11(&RANDSEED);

		// rotation is dir
		if(ps->mode.A.rotationIsDir) {
			struct point *p = &(particle->mode.A.dir);
			particle->rotation = -CC_RADIANS_TO_DEGREES(atan2f(p->y,p->x));
		}
	}
	// Mode Radius: B
	else {
		// Set the default diameter of the particle from the source position
		float startRadius = ps->mode.B.startRadius + ps->mode.B.startRadiusVar * RANDOM_M11(&RANDSEED);
		float endRadius = ps->mode.B.endRadius + ps->mode.B.endRadiusVar * RANDOM_M11(&RANDSEED);

		particle->mode.B.radius = startRadius;

		if (ps->mode.B.endRadius == START_RADIUS_EQUAL_TO_END_RADIUS) {
			particle->mode.B.deltaRadius = 0;
		} else {
			particle->mode.B.deltaRadius = (endRadius - startRadius) / particle->timeToLive;
		}

		particle->mode.B.angle = a;
		particle->mode.B.degreesPerSecond = CC_DEGREES_TO_RADIANS(ps->mode.B.rotatePerSecond + ps->mode.B.rotatePerSecondVar * RANDOM_M11(&RANDSEED));
	}
}

static void
_addParticle(struct particle_system *ps) {
	if (ps->particleCount == ps->totalParticles) {
		return;
	}

	struct particle * particle = &ps->particles[ ps->particleCount ];
	_initParticle(ps, particle);
	++ps->particleCount;
}

static void
_stopSystem(struct particle_system *ps) {
	ps->isActive = false;
	ps->elapsed = ps->duration;
	ps->emitCounter = 0;
}

static void
_normalize_point(struct point *p, struct point *out) {
	float l2 = p->x * p->x + p->y *p->y;
	if (l2 == 0) {
		out->x = 1.0f;
		out->y = 0;
	} else {
		float len = sqrtf(l2);
		out->x = p->x/len;
		out->y = p->y/len;
	}
}

static void
_update_particle(struct particle_system *ps, struct particle *p, float dt) {
	if (ps->positionType == POSITION_TYPE_RELATIVE) {
		p->startPos = ps->sourcePosition;
	}
	// Mode A: gravity, direction, tangential accel & radial accel
	if (ps->emitterMode == PARTICLE_MODE_GRAVITY) {
		struct point tmp, radial, tangential;

		radial.x = 0;
		radial.y = 0;
		// radial acceleration
		if (p->pos.x || p->pos.y) {
			_normalize_point(&p->pos, &radial);
		}
		tangential = radial;
		radial.x *= p->mode.A.radialAccel;
		radial.y *= p->mode.A.radialAccel;

		// tangential acceleration
		float newy = tangential.x;
		tangential.x = -tangential.y * p->mode.A.tangentialAccel;
		tangential.y = newy * p->mode.A.tangentialAccel;

		// (gravity + radial + tangential) * dt
		tmp.x = radial.x + tangential.x + ps->mode.A.gravity.x;
		tmp.y = radial.y + tangential.y + ps->mode.A.gravity.y;
		tmp.x *= dt;
		tmp.y *= dt;
		p->mode.A.dir.x += tmp.x;
		p->mode.A.dir.y += tmp.y;
		tmp.x = p->mode.A.dir.x * dt;
		tmp.y = p->mode.A.dir.y * dt;
		p->pos.x += tmp.x;
		p->pos.y += tmp.y;
	}
	// Mode B: radius movement
	else {
		// Update the angle and radius of the particle.
		p->mode.B.angle += p->mode.B.degreesPerSecond * dt;
		p->mode.B.radius += p->mode.B.deltaRadius * dt;

		p->pos.x = - cosf(p->mode.B.angle) * p->mode.B.radius;
		p->pos.y = - sinf(p->mode.B.angle) * p->mode.B.radius;
	}

	p->size += (p->deltaSize * dt);
	if (p->size < 0)
		p->size = 0;

	p->color.r += (p->deltaColor.r * dt);
	p->color.g += (p->deltaColor.g * dt);
	p->color.b += (p->deltaColor.b * dt);
	p->color.a += (p->deltaColor.a * dt);

	p->rotation += (p->deltaRotation * dt);
}

static void
_remove_particle(struct particle_system *ps, int idx) {
	if ( idx != ps->particleCount-1) {
		ps->particles[idx] = ps->particles[ps->particleCount-1];
	}
	--ps->particleCount;
}

void
init_with_particles(struct particle_system *ps, int numberOfParticles) {
	ps->totalParticles = numberOfParticles;
	ps->particles = (struct particle *)(ps+1);
	ps->matrix = (struct matrix *)(ps->particles + numberOfParticles);
	ps->allocatedParticles = numberOfParticles;
	ps->isActive = true;
	ps->positionType = POSITION_TYPE_FREE;
	ps->emitterMode = PARTICLE_MODE_GRAVITY;
}

void
calc_particle_system_mat(struct particle * p, struct matrix *m) {
	matrix_identity(m);
	struct srt srt;
	srt.rot = p->rotation * 1024 / 360;
	srt.scalex = p->size * SCREEN_SCALE;
	srt.scaley = srt.scalex;
	srt.offx = (p->pos.x + p->startPos.x) * SCREEN_SCALE;
	srt.offy = (p->pos.y + p->startPos.y) * SCREEN_SCALE;
	matrix_srt(m, &srt);
}

void
particle_system_update(struct particle_system *ps, float dt) {
	if (ps->isActive) {
		float rate = ps->emissionRate;

		// emitCounter should not increase where ps->particleCount == ps->totalParticles
		if (ps->particleCount < ps->totalParticles)	{
			ps->emitCounter += dt;
		}

		while (ps->particleCount < ps->totalParticles && ps->emitCounter > rate) {
			_addParticle(ps);
			ps->emitCounter -= rate;
		}

		ps->elapsed += dt;
		if (ps->duration != DURATION_INFINITY && ps->duration < ps->elapsed) {
			_stopSystem(ps);
		}
	}

	int i = 0;

	while (i < ps->particleCount) {
		struct particle *p = &ps->particles[i];
		p->timeToLive -= dt;
		if (p->timeToLive > 0) {
			_update_particle(ps,p,dt);
			if (p->size <= 0) {
				_remove_particle(ps, i);
			} else {
				++i;
			}
		} else {
			_remove_particle(ps, i);
		}
	}
}

