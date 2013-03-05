#include "RenderingSystem.h"
#include "RenderingSystem_Private.h"

#include "base/EntityManager.h"
#include <cmath>
#include <cassert>
#include <sstream>
#include <sys/select.h>
//~#include <sys/inotify.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../util/ImageLoader.h"

#ifdef EMSCRIPTEN
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_rwops.h>
#elif defined(ANDROID)
#include <GLES2/gl2ext.h>
#endif

RenderingSystem::TextureInfo::TextureInfo (const InternalTexture& ref,
        const Vector2& posInAtlas, const Vector2& sizeInAtlas, bool rot,
        const Vector2& atlasSize,
        const Vector2& offsetInOriginal, const Vector2& originalSize,
        const Vector2& _opaqueStart, const Vector2& _opaqueSize,
        int atlasIdx) {
	glref = ref;

	if (originalSize == Vector2::Zero) {
		uv[0].X = uv[0].Y = 0;
		uv[1].X = uv[1].Y = 1;
		rotateUV = 0;
	} else if (atlasIdx >= 0) {
		float blX = posInAtlas.X / atlasSize.X;
		float trX = (posInAtlas.X + sizeInAtlas.X) / atlasSize.X;
		float blY = 1 - (posInAtlas.Y + sizeInAtlas.Y) / atlasSize.Y;
		float trY = 1 - posInAtlas.Y / atlasSize.Y;

		uv[0].X = blX;
		uv[1].X = trX;
		uv[0].Y = blY;
		uv[1].Y = trY;
		rotateUV = rot;
	} else {
		uv[0].X = posInAtlas.X / atlasSize.X;
		uv[0].Y = posInAtlas.Y / atlasSize.Y;
		uv[1].X = (posInAtlas.X + sizeInAtlas.X) / atlasSize.X;
		uv[1].Y = (posInAtlas.Y + sizeInAtlas.Y) / atlasSize.Y;
		rotateUV = 0;
	}
	atlasIndex = atlasIdx;

    originalWidth = originalSize.X;
    originalHeight = originalSize.Y;

    Vector2 _sizeInAtlas(sizeInAtlas);
	if (rot) {
		std::swap(_sizeInAtlas.X, _sizeInAtlas.Y);
    }
    if (_sizeInAtlas.Y > 0) {
        opaqueSize = Vector2(_opaqueSize.X / _sizeInAtlas.X, _opaqueSize.Y / _sizeInAtlas.Y);
        opaqueStart = Vector2(_opaqueStart.X / _sizeInAtlas.X, 1 - (opaqueSize.Y + _opaqueStart.Y / _sizeInAtlas.Y));

        reduxSize = Vector2(_sizeInAtlas.X / originalSize.X, _sizeInAtlas.Y / originalSize.Y);
        reduxStart = Vector2(offsetInOriginal.X / originalSize.X, 1 - (reduxSize.Y + offsetInOriginal.Y / originalSize.Y));
    }
}

#include <fstream>

static void parse(const std::string& line, std::string& assetName, Vector2& originalSize, Vector2& reduxOffset, Vector2& posInAtlas, Vector2& sizeInAtlas, bool& rotate, Vector2& opaqueStart, Vector2& opaqueEnd) {
	std::string substrings[14];
	int from = 0, to = 0, count = 0;
	for (count=0; count<14; count++) {
		to = line.find_first_of(',', from);
		substrings[count] = line.substr(from, to - from);
        if (to == (int)std::string::npos) {
            count++;
            break;
        }
		from = to + 1;
	}
    // image,original width, original height, redux offset x, redux offset y, pox x in atlas, pos y in atlas, width, height, rotate[, opaque box min x, min y, max x, max y]
	assetName = substrings[0];
	originalSize.X = atoi(substrings[1].c_str());
	originalSize.Y = atoi(substrings[2].c_str());
	reduxOffset.X = atoi(substrings[3].c_str());
	reduxOffset.Y = atoi(substrings[4].c_str());
    posInAtlas.X = atoi(substrings[5].c_str());
    posInAtlas.Y = atoi(substrings[6].c_str());
    sizeInAtlas.X = atoi(substrings[7].c_str());
    sizeInAtlas.Y = atoi(substrings[8].c_str());
	rotate = atoi(substrings[9].c_str());
    if (count == 14) {
        opaqueStart.X = atoi(substrings[10].c_str());
        opaqueStart.Y = atoi(substrings[11].c_str());
        opaqueEnd.X = atoi(substrings[12].c_str());
        opaqueEnd.Y = atoi(substrings[13].c_str());
    }
}

void RenderingSystem::loadAtlas(const std::string& atlasName, bool forceImmediateTextureLoading) {
	std::string atlasDesc = atlasName + ".desc";
	std::string atlasImage = atlasName;

	FileBuffer file = assetAPI->loadAsset(atlasDesc);
	if (!file.data) {
		LOG(FATAL) << "Unable to load atlas description file '" << atlasDesc << "'";
		return;
	}

	Vector2 atlasSize, pow2Size;
	Atlas a;
	a.name = atlasImage;
	if (forceImmediateTextureLoading) {
		loadTexture(atlasName, atlasSize, pow2Size, a.glref);
	} else {
		a.glref = InternalTexture::Invalid;
	}
	atlas.push_back(a);
	int atlasIndex = atlas.size() - 1;

	std::stringstream f(std::string((const char*)file.data, file.size), std::ios_base::in);
	std::string s;
	f >> s;

	// read texture size
	sscanf(s.c_str(), "%f,%f", &atlasSize.X, &atlasSize.Y);
	VLOG(1) << "atlas '" << atlasName << "' -> index: " << atlasIndex;
	int count = 0;

	do {
		s.clear();
		f >> s;
		if (s.empty())
			break;
		count++;
		// LOGI("atlas - line: %s", s.c_str());
		std::string assetName;
        Vector2 originalSize, reduxOffset, posInAtlas, sizeInAtlas, opaqueStart(Vector2::Zero), opaqueEnd(Vector2::Zero);
		bool rot;
		parse(s, assetName, originalSize, reduxOffset, posInAtlas, sizeInAtlas, rot, opaqueStart, opaqueEnd);

		TextureRef result = nextValidRef++;
		VLOG(2) << "----- " << assetName << " -> " << result;
		assetTextures[assetName] = result;
		textures[result] = TextureInfo(a.glref, posInAtlas, sizeInAtlas, rot, atlasSize, reduxOffset, originalSize, opaqueStart, opaqueEnd - opaqueStart, atlasIndex);
	} while (!s.empty());

	delete[] file.data;
	VLOG(1) << "Atlas '" << atlasName << "' loaded " << count << " images";
}

void RenderingSystem::invalidateAtlasTextures() {
	for (unsigned int i=0; i<atlas.size(); i++) {
		atlas[i].glref = InternalTexture::Invalid;
	}
}

void RenderingSystem::unloadAtlas(const std::string& atlasName) {
	LOG(INFO) << "Unload atlas '" << atlasName << "' texture";
	for (unsigned int idx=0; idx<atlas.size(); idx++) {
		if (atlasName == atlas[idx].name) {
			for(std::map<TextureRef, TextureInfo>::iterator it=textures.begin(); it!=textures.end(); ++it) {
				if (it->second.atlasIndex == (int)idx) {
					it->second.glref = InternalTexture::Invalid;
				}
			}
			delayedDeletes.insert(atlas[idx].glref);
			atlas[idx].glref = InternalTexture::Invalid;
			break;
		}
	}
}

void RenderingSystem::loadTexture(const std::string& assetName, Vector2& realSize, Vector2& pow2Size, InternalTexture& out) {
	VLOG(1) << "loadTexture: '" << assetName << "'";

	/* create GL texture */
 	out.color = openGLTextureCreator.loadFromFile(assetAPI, assetName, realSize);
	out.alpha = openGLTextureCreator.loadFromFile(assetAPI, assetName + "_alpha", realSize);
    pow2Size = realSize;
}

void RenderingSystem::reloadTextures() {	
	LOG(INFO) << "Reloading textures begin";
	LOG(INFO) << "\t- atlas count: " << atlas.size();
	// reload atlas texture
	for (unsigned int i=0; i<atlas.size(); i++) {
		atlas[i].glref = InternalTexture::Invalid;
	}
    LOG(INFO) << "\t- textures count: " << assetTextures.size();
	for (std::map<std::string, TextureRef>::iterator it=assetTextures.begin(); it!=assetTextures.end(); ++it) {
		TextureInfo& info = textures[it->second];
		if (info.atlasIndex >= 0) {
			info.glref = atlas[info.atlasIndex].glref;
		} else {
            Vector2 size, psize;
			loadTexture(it->first, size, psize, info.glref);
		}
	}
	LOG(INFO) << "Reloading textures done";
}

void RenderingSystem::processDelayedTextureJobs() {
	PROFILE("Texture", "processDelayedTextureJobs", BeginEvent);
	#ifndef EMSCRIPTEN
	pthread_mutex_lock(&mutexes[L_TEXTURE]);
	#endif

	// load atlas
	for (std::set<int>::iterator it=delayedAtlasIndexLoad.begin(); it != delayedAtlasIndexLoad.end(); ++it) {
		int atlasIndex = *it;
		Vector2 atlasSize, pow2Size;
	    #ifndef EMSCRIPTEN
		pthread_mutex_unlock(&mutexes[L_TEXTURE]);
		#endif
		loadTexture(atlas[atlasIndex].name, atlasSize, pow2Size, atlas[atlasIndex].glref);
		VLOG(1) << "Atlas '" << atlas[atlasIndex].name << "' loaded (" << atlas[atlasIndex].glref.color << '/' << atlas[atlasIndex].glref.alpha << ')';

		for (std::map<std::string, TextureRef>::iterator jt=assetTextures.begin(); jt!=assetTextures.end(); ++jt) {
			TextureInfo& info = textures[jt->second];
			if (info.atlasIndex == atlasIndex) {
				info.glref = atlas[atlasIndex].glref;
			}
		}
	    #ifndef EMSCRIPTEN
        pthread_mutex_lock(&mutexes[L_TEXTURE]);
        #endif
	}
	delayedAtlasIndexLoad.clear();
/*
	// load textures
	for (std::set<std::string>::iterator it=delayedLoads.begin(); it != delayedLoads.end(); ++it) {
		Vector2 size, powSize;
		InternalTexture t;
		#ifndef EMSCRIPTEN
		pthread_mutex_unlock(&mutexes[L_TEXTURE]);
		#endif
		loadTexture(*it, size, powSize, t);
		textures[assetTextures[*it]] = TextureInfo(t, Vector2::Zero, 1+1, 1+1, size.X-1, size.Y-1, false, powSize);
		#ifndef EMSCRIPTEN
		pthread_mutex_lock(&mutexes[L_TEXTURE]);
		#endif
	}
	delayedLoads.clear();
*/
	// delete textures
	for (std::set<InternalTexture>::iterator it=delayedDeletes.begin(); it != delayedDeletes.end(); ++it) {
	    #ifndef EMSCRIPTEN
        pthread_mutex_unlock(&mutexes[L_TEXTURE]);
        #endif
		if (it->color != whiteTexture) {
			VLOG(1) << "Color texture delete:" << it->color;
			glDeleteTextures(1, &it->color);
		}
		if (it->alpha > 0 && it->alpha != whiteTexture) {
			VLOG(1) << "Alpha texture delete: " << it->alpha;
			glDeleteTextures(1, &it->alpha);
		}
	    #ifndef EMSCRIPTEN
        pthread_mutex_lock(&mutexes[L_TEXTURE]);
        #endif
	}
	delayedDeletes.clear();
	#ifndef EMSCRIPTEN
    pthread_mutex_unlock(&mutexes[L_TEXTURE]);
    #endif
	PROFILE("Texture", "processDelayedTextureJobs", EndEvent);
}

TextureRef RenderingSystem::loadTextureFile(const std::string& assetName) {
	PROFILE("Texture", "loadTextureFile", BeginEvent);
	TextureRef result = InvalidTextureRef;

	std::map<std::string, TextureRef>::const_iterator it = assetTextures.find(assetName);
	if (it != assetTextures.end()) {
		result = it->second;
	} else {
		LOG(FATAL) << "Texture '" << assetName << "' doesn't belong to any atlas. This is not supported";
        return InvalidTextureRef;
	}
	if (textures.find(result) == textures.end()) {
		PROFILE("RequestLoadTexture", assetName, InstantEvent);

		#ifndef EMSCRIPTEN
		pthread_mutex_lock(&mutexes[L_TEXTURE]);
		#endif
		delayedLoads.insert(assetName);
		#ifndef EMSCRIPTEN
		pthread_mutex_unlock(&mutexes[L_TEXTURE]);
		#endif
	}
	PROFILE("Texture", "loadTextureFile", EndEvent);
	return result;
}

Vector2 RenderingSystem::getTextureSize(const std::string& textureName) const {
	std::map<std::string, TextureRef>::const_iterator it = assetTextures.find(textureName);
	if (it != assetTextures.end()) {
        const TextureInfo& info = textures.find(it->second)->second;
        return Vector2(info.originalWidth, info.originalHeight);
	} else {
		LOG(FATAL) << "Unable to find texture '" << textureName << "'";
		return Vector2::Zero;
	}
}

void RenderingSystem::unloadTexture(TextureRef ref, bool allowUnloadAtlas) {
	if (ref != InvalidTextureRef) {
		TextureInfo i = textures[ref];
		if (i.atlasIndex >= 0 && !allowUnloadAtlas) {
			LOG(ERROR) << "Cannot delete texture '" << ref << "' (is an atlas";
			return;
		}
		for (std::map<std::string, TextureRef>::iterator it=assetTextures.begin(); it!=assetTextures.end(); ++it) {
			if (it->second == ref) {
				assetTextures.erase(it);
			    #ifndef EMSCRIPTEN
				pthread_mutex_lock(&mutexes[L_TEXTURE]);
				#endif
				delayedDeletes.insert(textures[ref].glref);
				#ifndef EMSCRIPTEN
				pthread_mutex_unlock(&mutexes[L_TEXTURE]);
				#endif
				break;
			}
		}
	} else {
		LOG(ERROR) << "Tried to delete an InvalidTextureRef";
	}
}

