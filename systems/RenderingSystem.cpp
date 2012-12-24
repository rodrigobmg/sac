#include "RenderingSystem.h"
#include "RenderingSystem_Private.h"
#include "base/EntityManager.h"
#include <cmath>
#include <cassert>
#include <sstream>
#if defined(ANDROID) || defined(EMSCRIPTEN)
#else
#include <sys/inotify.h>
#include <GL/glew.h>
#include <unistd.h>
#endif

RenderingSystem::InternalTexture RenderingSystem::InternalTexture::Invalid;

INSTANCE_IMPL(RenderingSystem);

RenderingSystem::RenderingSystem() : ComponentSystemImpl<RenderingComponent>("Rendering"), initDone(false) {
	nextValidRef = nextEffectRef = 1;
	currentWriteQueue = 0;
    frameQueueWritable = true;
    newFrameReady = false;
#ifndef EMSCRIPTEN
    pthread_mutex_init(&mutexes[L_RENDER], 0);
    pthread_mutex_init(&mutexes[L_QUEUE], 0);
    pthread_mutex_init(&mutexes[L_TEXTURE], 0);
	pthread_cond_init(&cond[0], 0);
    pthread_cond_init(&cond[1], 0);
#endif

    RenderingComponent tc;
    componentSerializer.add(new Property(OFFSET(texture, tc), sizeof(TextureRef)));
    componentSerializer.add(new Property(OFFSET(effectRef, tc), sizeof(EffectRef)));
    componentSerializer.add(new EpsilonProperty<float>(OFFSET(color.r, tc), 0.001));
    componentSerializer.add(new EpsilonProperty<float>(OFFSET(color.g, tc), 0.001));
    componentSerializer.add(new EpsilonProperty<float>(OFFSET(color.b, tc), 0.001));
    componentSerializer.add(new EpsilonProperty<float>(OFFSET(color.a, tc), 0.001));
    componentSerializer.add(new Property(OFFSET(hide, tc), sizeof(bool)));
    componentSerializer.add(new Property(OFFSET(mirrorH, tc), sizeof(bool)));
    componentSerializer.add(new Property(OFFSET(zPrePass, tc), sizeof(bool)));
    componentSerializer.add(new Property(OFFSET(fastCulling, tc), sizeof(bool)));
    componentSerializer.add(new Property(OFFSET(opaqueType, tc), sizeof(RenderingComponent::Opacity)));
    componentSerializer.add(new EpsilonProperty<float>(OFFSET(opaqueSeparation, tc), 0.001));
    componentSerializer.add(new Property(OFFSET(cameraBitMask, tc), sizeof(unsigned)));

    InternalTexture::Invalid.color = InternalTexture::Invalid.alpha = 0;
    initDone = true;
}

RenderingSystem::~RenderingSystem() {
#ifndef EMSCRIPTEN
    pthread_mutex_destroy(&mutexes[L_RENDER]);
    pthread_mutex_destroy(&mutexes[L_QUEUE]);
    pthread_mutex_destroy(&mutexes[L_TEXTURE]);
    pthread_cond_destroy(&cond[0]);
    pthread_cond_destroy(&cond[1]);
#endif
    initDone = false;
}

void RenderingSystem::setWindowSize(int width, int height, float sW, float sH) {
	windowW = width;
	windowH = height;
	screenW = sW;
	screenH = sH;
	GL_OPERATION(glViewport(0, 0, windowW, windowH))

    // create default camera
    cameras.push_back(Camera(Vector2::Zero, Vector2(screenW, screenH), Vector2::Zero, Vector2(1, 1)));
}

RenderingSystem::Shader RenderingSystem::buildShader(const std::string& vsName, const std::string& fsName) {
	Shader out;
	LOGI("building shader ...");
	out.program = glCreateProgram();
	check_GL_errors("glCreateProgram");

	LOGI("Compiling shaders: %s/%s\n", vsName.c_str(), fsName.c_str());
	GLuint vs = compileShader(vsName, GL_VERTEX_SHADER);
	GLuint fs = compileShader(fsName, GL_FRAGMENT_SHADER);

	GL_OPERATION(glAttachShader(out.program, vs))
	GL_OPERATION(glAttachShader(out.program, fs))
	LOGI("Binding GLSL attribs\n");
	GL_OPERATION(glBindAttribLocation(out.program, ATTRIB_VERTEX, "aPosition"))
	GL_OPERATION(glBindAttribLocation(out.program, ATTRIB_UV, "aTexCoord"))
	GL_OPERATION(glBindAttribLocation(out.program, ATTRIB_POS_ROT, "aPosRot"))

	LOGI("Linking GLSL program\n");
	GL_OPERATION(glLinkProgram(out.program))

	GLint logLength;
	glGetProgramiv(out.program, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 1) {
		char *log = new char[logLength];
		glGetProgramInfoLog(out.program, logLength, &logLength, log);
		LOGE("GL shader program error: '%s'", log);
		delete[] log;
	}

	out.uniformMatrix = glGetUniformLocation(out.program, "uMvp");
	out.uniformColorSampler = glGetUniformLocation(out.program, "tex0");
	out.uniformAlphaSampler = glGetUniformLocation(out.program, "tex1");
	out.uniformColor= glGetUniformLocation(out.program, "vColor");
#ifdef USE_VBO
	out.uniformUVScaleOffset = glGetUniformLocation(out.program, "uvScaleOffset");
	out.uniformRotation = glGetUniformLocation(out.program, "uRotation");
	out.uniformScaleZ = glGetUniformLocation(out.program, "uScaleZ");
#endif

	glDeleteShader(vs);
	glDeleteShader(fs);

	return out;
}

void RenderingSystem::init() {
	const GLubyte* extensions = glGetString(GL_EXTENSIONS);
	pvrSupported = (strstr((const char*)extensions, "GL_IMG_texture_compression_pvrtc") != 0);

	#ifdef USE_VBO
	defaultShader = buildShader("default_vbo.vs", "default.fs");
	defaultShaderNoAlpha = buildShader("default_vbo.vs", "default_no_alpha.fs");
    defaultShaderEmpty = buildShader("default_vbo.vs", "empty.fs");
	#else
	defaultShader = buildShader("default.vs", "default.fs");
	defaultShaderNoAlpha = buildShader("default.vs", "default_no_alpha.fs");
    defaultShaderEmpty = buildShader("default.vs", "empty.fs");
	#endif
	GL_OPERATION(glClearColor(148.0/255, 148.0/255, 148.0/255, 1.0)) // temp setting for RR

	// create 1px white texture
	uint8_t data[] = {255, 255, 255, 255};
	GL_OPERATION(glGenTextures(1, &whiteTexture))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, whiteTexture))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT))// CLAMP_TO_EDGE))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT))// CLAMP_TO_EDGE))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
	GL_OPERATION(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1,
                1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                data))

    /*GL_OPERATION(glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST))
    GL_OPERATION(glEnable(GL_CULL_FACE))
    GL_OPERATION(glShadeModel(GL_SMOOTH))*/

	// GL_OPERATION(glEnable(GL_BLEND))
	GL_OPERATION(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA))
	GL_OPERATION(glEnable(GL_DEPTH_TEST))
	GL_OPERATION(glDepthFunc(GL_GREATER))
#if defined(ANDROID) || defined(EMSCRIPTEN)
	GL_OPERATION(glClearDepthf(0.0))
 #else
    GL_OPERATION(glClearDepth(0.0))
 #endif
	// GL_OPERATION(glDepthRangef(0, 1))
	GL_OPERATION(glDepthMask(false))

#ifdef USE_VBO
	glGenBuffers(3, squareBuffers);
GLfloat sqArray[] = {
-0.5, -0.5, 0,0,0,
0.5, -0.5, 0,1,0,
-0.5, 0.5, 0,0,1,
0.5, 0.5, 0,1,1
};

GLfloat sqArrayRev[] = {
0.5, -0.5, 0,0,0,
0.5, 0.5, 0,1,0,
-0.5, -0.5, 0,0,1,
-0.5, 0.5, 0,1,1
};
unsigned short sqIndiceArray[] = {
	0,1,2,1,3,2
};
// Buffer d'informations de vertex
glBindBuffer(GL_ARRAY_BUFFER, squareBuffers[0]);
glBufferData(GL_ARRAY_BUFFER, sizeof(sqArray), sqArray, GL_STATIC_DRAW);

glBindBuffer(GL_ARRAY_BUFFER, squareBuffers[1]);
glBufferData(GL_ARRAY_BUFFER, sizeof(sqArrayRev), sqArrayRev, GL_STATIC_DRAW);

// Buffer d'indices
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, squareBuffers[2]);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sqIndiceArray), sqIndiceArray, GL_STATIC_DRAW);

#endif
}
static bool sortFrontToBack(const RenderingSystem::RenderCommand& r1, const RenderingSystem::RenderCommand& r2) {
	if (r1.z == r2.z) {
		if (r1.effectRef == r2.effectRef)
			return r1.texture < r2.texture;
		else
			return r1.effectRef < r2.effectRef;
	} else
		return r1.z > r2.z;
}

static bool sortBackToFront(const RenderingSystem::RenderCommand& r1, const RenderingSystem::RenderCommand& r2) {
    #define EPSILON 0.0001
	if (MathUtil::Abs(r1.z - r2.z) <= EPSILON) {
		if (r1.effectRef == r2.effectRef) {
			if (r1.texture == r2.texture) {
                if (r1.flags != r2.flags)
                    return r1.flags > r2.flags;
                else
	                return r1.color < r2.color;
	        } else {
	            return r1.texture < r2.texture;
	        }
		} else {
			return r1.effectRef < r2.effectRef;
		}
	} else {
		return r1.z < r2.z;
    }
}

static inline void modifyQ(RenderingSystem::RenderCommand& r, const Vector2& offsetPos, const Vector2& size) {
    const Vector2 offset =  offsetPos * r.halfSize * 2 + size * r.halfSize * 2 * 0.5;
    r.position = r.position  + Vector2((r.mirrorH ? -1 : 1), 1) * Vector2::Rotate(- r.halfSize + offset, r.rotation);
    r.halfSize = size * r.halfSize;
}

static void modifyR(RenderingSystem::RenderCommand& r, const Vector2& offsetPos, const Vector2& size) {
    const Vector2 offset =  offsetPos * r.halfSize * 2 + size * r.halfSize * 2 * 0.5;
    r.position = r.position  + Vector2((r.mirrorH ? -1 : 1), 1) * Vector2::Rotate(- r.halfSize + offset, r.rotation);
    r.halfSize = size * r.halfSize;
    r.uv[0] = offsetPos;
    r.uv[1] = size;
}

static bool cull(float camLeft, float camRight, RenderingSystem::RenderCommand& c) {
    if (c.rotation == 0) {
        assert (c.halfSize.X != 0);
        const float invWidth = 1.0f / (2 * c.halfSize.X);
        // left culling !
        {
            float cullLeftX = camLeft - (c.position.X - c.halfSize.X);
            if (cullLeftX > 0) {
                if (cullLeftX >= 2 * c.halfSize.X)
                    return false;
                const float prop = cullLeftX * invWidth; // € [0, 1]
                if (!c.mirrorH) {
                    c.uv[0].X += prop * c.uv[1].X;
                }
                c.uv[1].X *= (1 - prop);
                c.halfSize.X *= (1 - prop);
                c.position.X += 0.5 * cullLeftX;
                return true;
            }
        }
        // right culling !
        {
            float cullRightX = (c.position.X + c.halfSize.X) - camRight;
            if (cullRightX > 0) {
                if (cullRightX >= 2 * c.halfSize.X)
                    return false;
                const float prop = cullRightX * invWidth; // € [0, 1]
                if (c.mirrorH) {
                    c.uv[0].X += prop * c.uv[1].X;
                }
                c.uv[1].X *= (1 - prop);
                c.halfSize.X *= (1 - prop);
                c.position.X -= 0.5 * cullRightX;
                return true;
            }
        }
    }
    return true;
}

void RenderingSystem::DoUpdate(float) {
    static unsigned int cccc = 0;
    RenderQueue& outQueue = renderQueue[currentWriteQueue];
    if (outQueue.count != 0) {
        LOGW("Non empty queue : %d (queue=%d)", outQueue.count, currentWriteQueue);
    }
    outQueue.count = 0;
    for (int camIdx = cameras.size()-1; camIdx >= 0; camIdx--) {
        const Camera& camera = cameras[camIdx];
        if (!camera.enable)
            continue;
    	std::vector<RenderCommand> opaqueCommands, semiOpaqueCommands;
        const float camLeft = (camera.worldPosition.X - camera.worldSize.X * 0.5);
        const float camRight = (camera.worldPosition.X + camera.worldSize.X * 0.5);
    	/* render */
        FOR_EACH_ENTITY_COMPONENT(Rendering, a, rc)
            bool ccc = rc->cameraBitMask & (0x1 << camIdx);
    		if (rc->hide || rc->color.a <= 0 || !ccc) {
    			continue;
    		}

    		const TransformationComponent* tc = TRANSFORM(a);
            if (!isVisible(tc, camIdx)) {
                continue;
            }

    		RenderCommand c;
    		c.z = tc->worldZ;
    		c.texture = rc->texture;
    		c.effectRef = rc->effectRef;
    		c.halfSize = tc->size * 0.5f;
    		c.color = rc->color;
    		c.position = tc->worldPosition;
    		c.rotation = tc->worldRotation;
    		c.uv[0] = Vector2::Zero;
    		c.uv[1] = Vector2(1, 1);
            c.mirrorH = rc->mirrorH;
            if (rc->zPrePass) {
                c.flags = (EnableZWriteBit | DisableBlendingBit | DisableColorWriteBit);
            } else if (rc->opaqueType == RenderingComponent::FULL_OPAQUE) {
                c.flags = (EnableZWriteBit | DisableBlendingBit | EnableColorWriteBit);
            } else {
                c.flags = (DisableZWriteBit | EnableBlendingBit | EnableColorWriteBit);
            }

            if (c.texture != InvalidTextureRef) {
                const TextureInfo& info = textures[c.texture];
                int atlasIdx = info.atlasIndex;
                if (atlasIdx >= 0 && atlas[atlasIdx].glref == InternalTexture::Invalid) {
                    if (delayedAtlasIndexLoad.insert(atlasIdx).second) {
                        LOGW("Requested effective load of atlas '%s'", atlas[atlasIdx].name.c_str());
                    }
                }
                modifyQ(c, info.reduxStart, info.reduxSize);
                #ifndef USEE_VBO
                
                if (rc->opaqueType != RenderingComponent::FULL_OPAQUE &&
                    c.color.a >= 1 &&
                    info.opaqueSize != Vector2::Zero && 
                    !rc->zPrePass) {
                    // add a smaller full-opaque block at the center
                    RenderCommand cCenter(c);
                    cCenter.flags = (EnableZWriteBit | DisableBlendingBit | EnableColorWriteBit);
                    modifyR(cCenter, info.opaqueStart, info.opaqueSize);

                    if (cull(camLeft, camRight, cCenter))
                        opaqueCommands.push_back(cCenter);

                    const float leftBorder = info.opaqueStart.X, rightBorder = info.opaqueStart.X + info.opaqueSize.X;
                    const float bottomBorder = info.opaqueStart.Y + info.opaqueSize.Y;
                    if (leftBorder > 0) {
                        RenderCommand cLeft(c);
                        modifyR(cLeft, Vector2::Zero, Vector2(leftBorder, 1));
                        if (cull(camLeft, camRight, cLeft))
                            semiOpaqueCommands.push_back(cLeft);
                    }

                    if (rightBorder < 1) {
                        RenderCommand cRight(c);
                        modifyR(cRight, Vector2(rightBorder, 0), Vector2(1 - rightBorder, 1));
                        if (cull(camLeft, camRight, cRight))
                            semiOpaqueCommands.push_back(cRight);
                    }

                    RenderCommand cTop(c);
                    modifyR(cTop, Vector2(leftBorder, 0), Vector2(rightBorder - leftBorder, info.opaqueStart.Y));
                    if (cull(camLeft, camRight, cTop))
                        semiOpaqueCommands.push_back(cTop);

                    RenderCommand cBottom(c);
                    modifyR(cBottom, Vector2(leftBorder, bottomBorder), Vector2(rightBorder - leftBorder, 1 - bottomBorder));
                    if (cull(camLeft, camRight, cBottom))
                        semiOpaqueCommands.push_back(cBottom);
                    continue;
                }
                #endif
             }

             #ifndef USE_VBO
             if (!rc->fastCulling) {
                if (!cull(camLeft, camRight, c)) {
                    continue;
                }
             }
             #endif
             switch (rc->opaqueType) {
             	case RenderingComponent::NON_OPAQUE:
    	         	semiOpaqueCommands.push_back(c);
    	         	break;
    	         case RenderingComponent::FULL_OPAQUE:
    	         	opaqueCommands.push_back(c);
    	         	break;
                 default:
                    LOGW("Entity will not be drawn");
                    break;
             }
        }

    	std::sort(opaqueCommands.begin(), opaqueCommands.end(), sortFrontToBack);
    	std::sort(semiOpaqueCommands.begin(), semiOpaqueCommands.end(), sortBackToFront);

        RenderCommand dummy;
        dummy.texture = BeginFrameMarker;
        dummy.halfSize = camera.worldPosition;
        dummy.uv[0] = camera.worldSize;
        dummy.uv[1] = camera.screenPosition;
        dummy.position = camera.screenSize;
        dummy.effectRef = camera.mirrorY;
        dummy.rotateUV = cccc;
        outQueue.commands[0] = dummy;// outQueue.commands.push_back(dummy);
        std::copy(opaqueCommands.begin(), opaqueCommands.end(), &outQueue.commands[1]);
        outQueue.count = 1 + opaqueCommands.size();
        // semiOpaqueCommands.front().flags = (DisableZWriteBit | EnableBlendingBit);
        std::copy(semiOpaqueCommands.begin(), semiOpaqueCommands.end(), &outQueue.commands[outQueue.count]);
        outQueue.count += semiOpaqueCommands.size();
    }
    RenderCommand dummy;
    dummy.texture = EndFrameMarker;
    dummy.rotateUV = cccc;
    outQueue.commands[outQueue.count++] = dummy;
    std::stringstream framename;
    framename << "create-frame-" << cccc;
    PROFILE("Render", framename.str(), InstantEvent);
    cccc++;
	//LOGW("[%d] Added: %d + %d + 2 elt (%d frames) -> %d (%u)", currentWriteQueue, opaqueCommands.size(), semiOpaqueCommands.size(), outQueue.frameToRender, outQueue.commands.size(), dummy.rotateUV);
    //LOGW("Wrote frame %d commands to queue %d", outQueue.count, currentWriteQueue);

#ifndef EMSCRIPTEN
    // Lock to not change queue while ther thread is reading it
    pthread_mutex_lock(&mutexes[L_RENDER]);
    // Lock for notifying queue change
    pthread_mutex_lock(&mutexes[L_QUEUE]);
#endif
    currentWriteQueue = (currentWriteQueue + 1) % 2;
    newFrameReady = true;
#ifndef EMSCRIPTEN
    pthread_cond_signal(&cond[C_FRAME_READY]);
    pthread_mutex_unlock(&mutexes[L_QUEUE]);
    pthread_mutex_unlock(&mutexes[L_RENDER]);
#endif
}

bool RenderingSystem::isEntityVisible(Entity e, int cameraIndex) const {
	return isVisible(TRANSFORM(e), cameraIndex);
}

bool RenderingSystem::isVisible(const TransformationComponent* tc, int cameraIndex) const {
    if (cameraIndex < 0) {
        for (unsigned camIdx = 0; camIdx < cameras.size(); camIdx++) {
            if (isVisible(tc, camIdx)) {
                return true;
            }
        }
        return false;
    }
    const Camera& camera = cameras[cameraIndex];
	const Vector2 halfSize(tc->size * 0.5);
	const Vector2 pos(tc->worldPosition - camera.worldPosition);
    const Vector2 camHalfSize(camera.worldSize * .5);

	if ((pos.X + halfSize.X) < -camHalfSize.X) return false;
	if ((pos.X - halfSize.X) > camHalfSize.X) return false;
	if ((pos.Y + halfSize.Y) < -camHalfSize.Y) return false;
	if ((pos.Y - halfSize.Y) > camHalfSize.Y) return false;
	return true;
}

int RenderingSystem::saveInternalState(uint8_t** out) {
	int size = 0;
    size += sizeof(int) + cameras.size() * sizeof(Camera);
	for (std::map<std::string, TextureRef>::iterator it=assetTextures.begin(); it!=assetTextures.end(); ++it) {
		size += (*it).first.length() + 1;
		size += sizeof(TextureRef);
		size += sizeof(TextureInfo);
	}

	*out = new uint8_t[size];
	uint8_t* ptr = *out;
    int camCount = cameras.size();
    ptr = (uint8_t*) mempcpy(ptr, &camCount, sizeof(int));
    for (int i=0; i<camCount; i++) {
        ptr = (uint8_t*) mempcpy(ptr, &cameras[i], sizeof(Camera));
    }
	for (std::map<std::string, TextureRef>::iterator it=assetTextures.begin(); it!=assetTextures.end(); ++it) {
		ptr = (uint8_t*) mempcpy(ptr, (*it).first.c_str(), (*it).first.length() + 1);
		ptr = (uint8_t*) mempcpy(ptr, &(*it).second, sizeof(TextureRef));
		ptr = (uint8_t*) mempcpy(ptr, &(textures[it->second]), sizeof(TextureInfo));
	}
	return size;
}

void RenderingSystem::restoreInternalState(const uint8_t* in, int size) {
	assetTextures.clear();
	textures.clear();
	nextValidRef = 1;
	nextEffectRef = 1;
	int idx = 0;
    int camCount = 0;
    cameras.clear();
    memcpy(&camCount, &in[idx], sizeof(int));
    idx += sizeof(int);
    for (int i=0; i<camCount; i++) {
        Camera cam;
        memcpy(&cam, &in[idx], sizeof(Camera));
        idx += sizeof(Camera);
        cameras.push_back(cam);
    }
	while (idx < size) {
		char name[128];
		int i=0;
		do {
			name[i] = in[idx++];
		} while (name[i++] != '\0');
		TextureRef ref;
		memcpy(&ref, &in[idx], sizeof(TextureRef));
		idx += sizeof(TextureRef);
		TextureInfo info;
		memcpy(&info, &in[idx], sizeof(TextureInfo));
		idx += sizeof(TextureInfo);

		assetTextures[name] = ref;
		if (info.atlasIndex >= 0) {
			info.glref = atlas[info.atlasIndex].glref;
			textures[ref] = info;
		}
		nextValidRef = MathUtil::Max(nextValidRef, ref + 1);
	}
	for (std::map<std::string, EffectRef>::iterator it=nameToEffectRefs.begin();
		it != nameToEffectRefs.end();
		++it) {
		nextEffectRef = MathUtil::Max(nextEffectRef, it->second + 1);
	}
}

EffectRef RenderingSystem::loadEffectFile(const std::string& assetName) {
	EffectRef result = DefaultEffectRef;
	std::string name(assetName);

	if (nameToEffectRefs.find(name) != nameToEffectRefs.end()) {
		result = nameToEffectRefs[name];
		return result;
	} else {
		result = nextEffectRef++;
		nameToEffectRefs[name] = result;
	}

	#ifdef USE_VBO
	ref2Effects[result] = buildShader("default_vbo.vs", assetName);
	#else
	ref2Effects[result] = buildShader("default.vs", assetName);
	#endif

	return result;
}

void RenderingSystem::reloadEffects() {
	for (std::map<std::string, EffectRef>::iterator it=nameToEffectRefs.begin();
		it != nameToEffectRefs.end();
		++it) {
		#ifdef USE_VBO
		ref2Effects[it->second] = buildShader("default_vbo.vs", it->first);
		#else
		ref2Effects[it->second] = buildShader("default.vs", it->first);
		#endif
	}
}

void RenderingSystem::setFrameQueueWritable(bool b) {
    if (frameQueueWritable == b || !initDone)
        return;
    LOGI("Writable: %d", b);
#ifndef EMSCRIPTEN
    pthread_mutex_lock(&mutexes[L_QUEUE]);
#endif
    frameQueueWritable = b;
#ifndef EMSCRIPTEN
    pthread_cond_signal(&cond[C_FRAME_READY]);
    pthread_mutex_unlock(&mutexes[L_QUEUE]);

    pthread_mutex_lock(&mutexes[L_RENDER]);
    pthread_cond_signal(&cond[C_RENDER_DONE]);
    pthread_mutex_unlock(&mutexes[L_RENDER]);
#endif
}
