#include "pch.h"
#include "../../Defines.h"

#include "MaterialGenerator.h"
#include "MaterialDefinition.h"
#include "MaterialFactory.h"
#include "ShaderProperties.h"

#ifndef ROAD_EDITOR
	#include "../../OgreGame.h"
#else
	#include "../../../editor/OgreApp.h"
#endif


#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreHighLevelGpuProgram.h>
#include <OgreGpuProgramParams.h>
#include <OgreRoot.h>
using namespace Ogre;

bool MaterialGenerator::bUseMRT=false;	


void MaterialGenerator::generate(bool fixedFunction)
{	
	mMaterial = prepareMaterial(mDef->getName());
	
	// reset some attributes
	resetTexUnitCounter();
	
	// choose textures from list (depending on user iTexSize setting)
	chooseTextures();
	
	// -------------------------- Main technique ----------------------------- //
	Ogre::Technique* technique = mMaterial->createTechnique();
	
	// Main pass
	Ogre::Pass* pass = technique->createPass();
	
	pass->setAmbient( mDef->mProps->ambient.x, mDef->mProps->ambient.y, mDef->mProps->ambient.z );
	pass->setDiffuse( mDef->mProps->diffuse.x, mDef->mProps->diffuse.y, mDef->mProps->diffuse.z, 1.0 );
	
	if (!needShaders() || fixedFunction)
	{
		pass->setSpecular(mDef->mProps->specular.x, mDef->mProps->specular.y, mDef->mProps->specular.z, 1.0 );
		pass->setShininess(mDef->mProps->specular.w);
	}
	else
	{
		// shader assumes shininess in specular w component
		pass->setSpecular(mDef->mProps->specular.x, mDef->mProps->specular.y, mDef->mProps->specular.z, mDef->mProps->specular.w);
	}
	
	pass->setCullingMode(chooseCullingMode());
	
	if (!mDef->mProps->fog)
		pass->setFog(true); // actually this disables fog
		
	if (!mDef->mProps->lighting)
		pass->setLightingEnabled(false);
	
	if (mDef->mProps->sceneBlend == SBM_ALPHA_BLEND)
		pass->setSceneBlending(SBT_TRANSPARENT_ALPHA);
	else if (mDef->mProps->sceneBlend == SBM_COLOUR_BLEND)
		pass->setSceneBlending(SBT_TRANSPARENT_COLOUR);
	else if (mDef->mProps->sceneBlend == SBM_ADD)
		pass->setSceneBlending(SBT_ADD);
	else if (mDef->mProps->sceneBlend == SBM_MODULATE)
		pass->setSceneBlending(SBT_MODULATE);
		
	pass->setDepthWriteEnabled( mDef->mProps->depthWrite );
		
	pass->setDepthCheckEnabled( mDef->mProps->depthCheck );
		
	pass->setTransparentSortingEnabled( mDef->mProps->transparentSorting );
	
	pass->setAlphaRejectFunction( mDef->mProps->alphaRejectFunc );
	pass->setAlphaRejectValue( mDef->mProps->alphaRejectValue );
	
	if (mDef->mProps->depthBias != 0.f)
		pass->setDepthBias( mDef->mProps->depthBias );
	
	if (!needShaders() || fixedFunction)
		createTexUnits(pass, false);
	else
	{
		createTexUnits(pass, true);
		
		// create shaders		
		if (!mShaderCached)
		{
			try
			{
				mVertexProgram = createVertexProgram();
				mFragmentProgram = createFragmentProgram();
			}
			catch (Ogre::Exception& e) {
				LogO(e.getFullDescription());
			}
			
			if (mFragmentProgram.isNull() || mVertexProgram.isNull() || 
				!mFragmentProgram->isSupported() || !mVertexProgram->isSupported())
			{
				LogO("[MaterialFactory] WARNING: shader for material '" + mDef->getName()
					+ "' is not supported, falling back to fixed-function");
				LogO("[MaterialFactory] Vertex program source: ");
				StringUtil::StrStreamType vSourceStr;
				generateVertexProgramSource(vSourceStr);
				LogO(vSourceStr.str());
				LogO("[MaterialFactory] Fragment program source: ");
				StringUtil::StrStreamType fSourceStr;
				generateFragmentProgramSource(fSourceStr);
				LogO(fSourceStr.str());
				
				mVertexProgram.setNull(); mFragmentProgram.setNull();
				generate(true);
				return;
			}
		}
		
		pass->setVertexProgram(mVertexProgram->getName());
		pass->setFragmentProgram(mFragmentProgram->getName());
		
		//set shadow caster
		technique->setShadowCasterMaterial(chooseShadowCasterMaterial());
		if (mShaderCached)
		{
			// set individual material shader params
			individualVertexProgramParams(pass->getVertexProgramParameters());
			individualFragmentProgramParams(pass->getFragmentProgramParameters());
		}
	}
	// ----------------------------------------------------------------------- //
	
	createSSAOTechnique();
	
	// indicate that we need the pssm split points
	if (needShadows())
		mParent->splitMtrs.push_back( mDef->getName() );
		
	// indicate that we need terrain lightmap texture and terrainWorldSize
	if (needTerrainLightMap())
		mParent->terrainLightMapMtrs.push_back( mDef->getName() );
		
	// indicate we need enable/disable fog parameter
	if (mDef->mProps->fog)
		mParent->fogMtrs.push_back( mDef->getName() );
		
	// indicate we need enable/disable wind parameter
	// only needed for trees (wind == 2) because they have impostors
	if (mShader->wind == 2)
		mParent->windMtrs.push_back( mDef->getName() );
	
	// uncomment to export to .material
	/*
	if (mDef->getName() == "pipeGlass") {
	MaterialSerializer serializer;
	serializer.exportMaterial(mMaterial, "test.material");
	}
	*/
	
	// uncomment to see full shader source code in log
	/*
	if (mDef->getName() == "Particles/Spark")
	{
		LogO("[MaterialFactory] Vertex program source: ");
		StringUtil::StrStreamType vSourceStr;
		generateVertexProgramSource(vSourceStr);
		LogO(vSourceStr.str());
		LogO("[MaterialFactory] Fragment program source: ");
		StringUtil::StrStreamType fSourceStr;
		generateFragmentProgramSource(fSourceStr);
		LogO(fSourceStr.str());
	}
	*/
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::createTexUnits(Ogre::Pass* pass, bool shaders)
{
	Ogre::TextureUnitState* tu;
	
	// diffuse / light / blend maps
	if (needDiffuseMap())
	{
		tu = pass->createTextureUnitState( mDiffuseMap );
		tu->setName("diffuseMap");
		tu->setTextureAddressingMode(mDef->mProps->textureAddressMode);
		mDiffuseTexUnit = mTexUnit_i; mTexUnit_i++;
	}
	if (needLightMap())
	{
		tu = pass->createTextureUnitState( mLightMap );
		tu->setName("lightMap");
		tu->setTextureAddressingMode(mDef->mProps->textureAddressMode);
		mLightTexUnit = mTexUnit_i; mTexUnit_i++;
	}
	if (needBlendMap())
	{
		tu = pass->createTextureUnitState( mBlendMap );
		tu->setName("blendMap");
		tu->setTextureAddressingMode(mDef->mProps->textureAddressMode);
		mBlendTexUnit = mTexUnit_i; mTexUnit_i++;
	}
	
	// spec map
	if (needSpecMap())
	{
		tu = pass->createTextureUnitState( mSpecMap );
		tu->setName("specMap");
		tu->setTextureAddressingMode(mDef->mProps->textureAddressMode);
		mSpecTexUnit = mTexUnit_i; mTexUnit_i++;
	}
	
	// reflectivity map
	if (needReflectivityMap())
	{
		tu = pass->createTextureUnitState( mReflMap );
		tu->setName("reflectivityMap");
		tu->setTextureAddressingMode(mDef->mProps->textureAddressMode);
		mReflTexUnit = mTexUnit_i; mTexUnit_i++;
	}
	
	// global terrain lightmap (static)
	if (needTerrainLightMap())
	{
		tu = pass->createTextureUnitState("white.png"); // texture name set later (in changeShadows)
		tu->setName("terrainLightMap");
		mTerrainLightTexUnit = mTexUnit_i; mTexUnit_i++;
	}
	
	// alpha map
	if (needAlphaMap())
	{
		tu = pass->createTextureUnitState( mAlphaMap );
		tu->setName("alphaMap");
		mAlphaTexUnit = mTexUnit_i; mTexUnit_i++;
	}
	
	// normal map
	if (needNormalMap())
	{
		tu = pass->createTextureUnitState( mNormalMap );
		tu->setName("normalMap");
		mNormalTexUnit = mTexUnit_i; mTexUnit_i++;
	}

	// env map
	if (needEnvMap())
	{
		tu = pass->createTextureUnitState( mDef->mProps->envMap );
		tu->setName("envMap");
		tu->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);
		mEnvTexUnit = mTexUnit_i; mTexUnit_i++;
	}

	
	// realtime shadow maps
	if (needShadows())
	{
		mShadowTexUnit_start = mTexUnit_i;
		for (int i = 0; i < mParent->getNumShadowTex(); ++i)
		{
			tu = pass->createTextureUnitState();
			tu->setName("shadowMap" + toStr(i));
			tu->setContentType(TextureUnitState::CONTENT_SHADOW);
			tu->setTextureAddressingMode(TextureUnitState::TAM_BORDER);
			tu->setTextureBorderColour(ColourValue::White);
			mTexUnit_i++;
		}
	}
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::resetTexUnitCounter()
{
	mDiffuseTexUnit = 0; mNormalTexUnit = 0; mEnvTexUnit = 0; mAlphaTexUnit = 0;
	mShadowTexUnit_start = 0; mTerrainLightTexUnit = 0; mTexUnit_i = 0;
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::createSSAOTechnique()
{
	Technique* ssaopasstech = mMaterial->createTechnique();
	ssaopasstech->setName("geom");
	ssaopasstech->setSchemeName("geom");
	Pass* ssaopass = ssaopasstech->createPass();

	HighLevelGpuProgramManager& hmgr = HighLevelGpuProgramManager::getSingleton();
	
	// choose vertex program
	std::string vprogname = "geom_vs";
	if ( mDef->mProps->transparent )
		vprogname = "geom_coord_vs";
	
	// choose fragment program
	std::string fprogname = "geom_ps";
	if ( !mDef->mProps->ssao )
		fprogname = "geom_white_ps"; // no contribution to ssao, will just return (0,0,0,0)
	else if ( mDef->mProps->transparent )
		fprogname = "geom_alpha_ps";
	
	ssaopass->setVertexProgram(vprogname);
	ssaopass->setFragmentProgram(fprogname);
	
	if (mDef->mProps->ssaoReject)
		ssaopass->setAlphaRejectSettings(CMPF_GREATER, 128);
	
	if ( !mDef->mProps->transparent ) 
	{
		ssaopass->setCullingMode( chooseCullingMode() );
	}
	else
	{
		ssaopass->setCullingMode( CULL_NONE );
		ssaopass->setAlphaRejectSettings(CMPF_GREATER_EQUAL, 128);
		ssaopass->createTextureUnitState( mDiffuseMap );
	}
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::chooseTextures()
{
	mDiffuseMap = pickTexture(&mDef->mProps->diffuseMaps);
	mNormalMap = pickTexture(&mDef->mProps->normalMaps);
	mLightMap = pickTexture(&mDef->mProps->lightMaps);
	mAlphaMap = pickTexture(&mDef->mProps->alphaMaps);
	mBlendMap = pickTexture(&mDef->mProps->blendMaps);
	mSpecMap = pickTexture(&mDef->mProps->specMaps);
	mReflMap = pickTexture(&mDef->mProps->reflectivityMaps);
}

//----------------------------------------------------------------------------------------

MaterialPtr MaterialGenerator::prepareMaterial(const std::string& name)
{
	MaterialPtr mat;
	
	if (MaterialManager::getSingleton().resourceExists(name))
		mat = MaterialManager::getSingleton().getByName(name);
	else
		mat = MaterialManager::getSingleton().create(name, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
	
	mat->removeAllTechniques();
	
	return mat;
}

//----------------------------------------------------------------------------------------

bool MaterialGenerator::needShaders()
{
	return mParent->getShaders() && mDef->mProps->shaders;
}

bool MaterialGenerator::needShadows()
{
	return mShader->shadows;
}

bool MaterialGenerator::needNormalMap()
{
	return mShader->normalMap;
}

bool MaterialGenerator::needEnvMap()
{
	return mShader->envMap;
}

bool MaterialGenerator::needDiffuseMap()
{
	return mShader->diffuseMap;
}

bool MaterialGenerator::needLightMap()
{
	return mShader->lightMap;
}

bool MaterialGenerator::needTerrainLightMap()
{
	// temporary workaround. terrain lightmap is broken when depth shadows off (texture not found).
	return mShader->terrainLightMap && mParent->getShadowsDepth();
}

bool MaterialGenerator::needBlendMap()
{
	return mShader->blendMap;
}

bool MaterialGenerator::needSpecMap()
{
	return mShader->specMap;
}

bool MaterialGenerator::needReflectivityMap()
{
	return mShader->reflectivityMap;
}

bool MaterialGenerator::needLightingAlpha()
{
	return mShader->lightingAlpha;
}

bool MaterialGenerator::needAlphaMap()
{
	return mShader->alphaMap;
}

bool MaterialGenerator::needFresnel()
{
	return mShader->fresnel;
}

bool MaterialGenerator::fpNeedLighting()
{
	return mShader->lighting;
}

bool MaterialGenerator::fpNeedWsNormal()
{
	return needEnvMap() || needNormalMap() || fpNeedLighting() || needTerrainLightMap() || MRTSupported();
}

bool MaterialGenerator::fpNeedEyeVector()
{
	return needEnvMap() || fpNeedLighting();
}

bool MaterialGenerator::vpNeedTangent()
{
	return needNormalMap();
}

bool MaterialGenerator::vpNeedWMat()
{
	return fpNeedEyeVector() || needTerrainLightMap();
}
bool MaterialGenerator::fpNeedWMat()
{
	return UsePerPixelNormals();
}
bool MaterialGenerator::vpNeedWvMat()
{
	return MRTSupported();
}
bool MaterialGenerator::UsePerPixelNormals()
{
	return false;//this is not working at the moment
}
bool MaterialGenerator::MRTSupported()
{
	//return false; //! MRT is still buggy
	
	static bool bMRTSupportCalculated=false;
	if(!bMRTSupportCalculated)
	{
		const RenderSystemCapabilities *caps = Root::getSingleton().getRenderSystem()->getCapabilities();
		if(caps->isShaderProfileSupported("ps_3_0") 
		//	|| caps->isShaderProfileSupported("ps_4_0")
		//	|| caps->isShaderProfileSupported("fp40")
			)
		{
			bUseMRT=true;
		}
		bMRTSupportCalculated=true;
	}
	return bUseMRT;
}

bool MaterialGenerator::vpNeedWITMat()
{
	return fpNeedWsNormal();
}

bool MaterialGenerator::fpNeedTangentToCube()
{
	return (needNormalMap() || fpNeedEyeVector());
}

std::string MaterialGenerator::getChannel(unsigned int n)
{
	if (n == 0) 		return "x";
	else if (n == 1) 	return "y";
	else if (n == 2)	return "z";
	else 				return "w";
}

//----------------------------------------------------------------------------------------

std::string MaterialGenerator::pickTexture(textureMap* textures)
{
	if (textures->size() == 0) return "";
	
	// we assume the textures are sorted by size
	textureMap::iterator it;
	for (it = textures->begin(); it != textures->end(); ++it)
	{
		if ( it->first < mParent->getTexSize() ) continue;
		/* else */ break;
	}
	
	if (it == textures->end()) --it;
	
	return it->second;
}


//----------------------------------------------------------------------------------------

Ogre::CullingMode MaterialGenerator::chooseCullingMode()
{

	if 		(mDef->mProps->cullHardware == CULL_HW_NONE)
		return Ogre::CULL_NONE;
	else if (mDef->mProps->cullHardware == CULL_HW_CLOCKWISE)
		return Ogre::CULL_CLOCKWISE;
	else if (mDef->mProps->cullHardware == CULL_HW_ANTICLOCKWISE)
		return Ogre::CULL_ANTICLOCKWISE;
	else if (mDef->mProps->cullHardware == CULL_HW_CLOCKWISE_OR_NONE)
	{
		if (mParent->getShadowsDepth())
			return Ogre::CULL_NONE;
		else
			return Ogre::CULL_CLOCKWISE;
	}
	else if (mDef->mProps->cullHardware == CULL_HW_ANTICLOCKWISE_OR_NONE)
	{
		if (mParent->getShadowsDepth())
			return Ogre::CULL_NONE;
		else
			return Ogre::CULL_ANTICLOCKWISE;
	}
	return Ogre::CULL_NONE;
}

//----------------------------------------------------------------------------------------

Ogre::String MaterialGenerator::chooseShadowCasterMaterial()
{
	Ogre::String shadowCasterMaterial = StringUtil::BLANK;
	if(!mDef->mProps->transparent)
	{
		if(mParent->getShadowsDepth())
		{
			Ogre::CullingMode cmode = chooseCullingMode();
			if(cmode == Ogre::CULL_NONE)
			{
				shadowCasterMaterial = "PSSM/shadow_caster_nocull";				
			}
			else
			{
				shadowCasterMaterial = "PSSM/shadow_caster_noalpha";
			}
		}
	}
	return shadowCasterMaterial;
}
//----------------------------------------------------------------------------------------

HighLevelGpuProgramPtr MaterialGenerator::createVertexProgram()
{
	HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
	std::string progName = mDef->getName() + "_VP";

	HighLevelGpuProgramPtr ret = mgr.getByName(progName);
	if (!ret.isNull())
		mgr.remove(progName);

	ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
		"cg", GPT_VERTEX_PROGRAM);

	if(MRTSupported())
	{
		ret->setParameter("profiles", "vs_4_0 vs_3_0 vp40");
	}
	else
	{
		ret->setParameter("profiles", "vs_4_0 vs_2_x arbvp1");
	}
	ret->setParameter("entry_point", "main_vp");

	StringUtil::StrStreamType sourceStr;
	generateVertexProgramSource(sourceStr);
	ret->setSource(sourceStr.str());
	ret->load();
	vertexProgramParams(ret);
	
	return ret;
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::vpShadowingParams(Ogre::StringUtil::StrStreamType& outStream)
{
	
	
	if (needShadows()) {
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream << "out float4 oLightPosition"+toStr(i)+" : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
		}
		outStream << "\n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream << "uniform float4x4 texWorldViewProjMatrix"+toStr(i)+", \n";
		}
		outStream << "\n";
	}
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::generateVertexProgramSource(Ogre::StringUtil::StrStreamType& outStream)
{
	// note: world position xz for fragment is stored in oTexCoord.w, oWsNormal.w
	mTexCoord_i=0;
	
	outStream << 
		"void main_vp( "
		"	float4 position 					: POSITION, \n";
	
	if (fpNeedWsNormal()) 
	{
		outStream <<"	float3 normal			 			: NORMAL, \n";
	}
	if (vpNeedTangent()) outStream <<
		"	float3 tangent						: TANGENT, \n";
	outStream << 
		"	float2 texCoord 					: TEXCOORD0, \n";
	
	if (fpNeedEyeVector()) outStream <<
		"	uniform float4 eyePosition,	 \n";
	outStream <<
		"	out float4 oPosition			 	: POSITION, \n"
		"	out float4 objectPos				: COLOR, \n" // running out of texcoords so putting this in COLOR since its unused.
		"	out float4 oTexCoord				: TEXCOORD"+toStr(mTexCoord_i++)+", \n";
		
	if (mShader->vertexColour) outStream <<
		"	float4 color 						: COLOR, \n"
		"	out float4 oVertexColour				: TEXCOORD"+toStr(mTexCoord_i++)+", \n";
		
	if (mShader->wind == 1)
	{
		outStream <<
		"	uniform float time, \n"
		"	uniform float frequency, \n"
		"	uniform float3 objSpaceCam, \n"
		"	uniform float fadeRange, \n"
		"	uniform float4 direction, \n"
		"	out float alphaFade : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
	}
	else if (mShader->wind == 2)
	{
		outStream <<
		"	uniform float time, \n"
		"	uniform float enableWind, \n"
		"	float4 windParams 	: TEXCOORD1, \n"
		"	float4 originPos 	: TEXCOORD2, \n";
	}

	if (fpNeedWsNormal()) 
	{
		if(UsePerPixelNormals())
		{
			outStream <<"	out float4 oNormal  				: COLOR1, \n";
		}
		else
		{
			if(MRTSupported())
			{
				outStream <<"	out float4 oWsNormal  				: COLOR1, \n";
			}
			else
			{
				outStream <<"	out float4 oWsNormal  				: TEXCOORD"+toStr(mTexCoord_i++)+", \n";	
			}
		}
	}
	if (needNormalMap()) outStream <<
		"	uniform float bumpScale, \n";
		
	if (fpNeedTangentToCube()) 
	{
		 outStream <<"	out	float4	oTangentToCubeSpace0	: TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n"; // tangent to cube (world) space
		 outStream <<"	out	float4	oTangentToCubeSpace1	: TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n";
		 outStream <<"	out	float4	oTangentToCubeSpace2	: TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n";
	}
	if(MRTSupported())
	{
		if(!UsePerPixelNormals())
		{
			//view space normal 
			outStream << "	out	float4	oViewNormal	: TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n";
		}
	}

	// fog
	outStream <<
		"	uniform float enableFog, \n"
		"	uniform float4 fogParams, \n";
		
	vpShadowingParams(outStream);

	if (vpNeedWITMat()) outStream <<
		"	uniform float4x4 wITMat, \n";
	if (vpNeedWvMat()) outStream <<
		"	uniform float4x4 wvMat, \n";
	if (vpNeedWMat()) outStream <<
		"	uniform float4x4 wMat, \n";
	outStream << 
	"	uniform float4x4 wvpMat \n"
	") \n"
	"{ \n";
	
	if (mShader->vertexColour) outStream <<
		"	oVertexColour = color; \n";
	
	if (mShader->wind == 1)
	{
		// wave
		outStream <<
		"	float oldposx = position.x; \n"
		"	if (texCoord.y == 0.0f) \n"
		"	{ \n"
		"		float offset = sin(time + oldposx * frequency); \n"
		"		position += direction * offset; \n"
		"	} \n"
		
		// fade
		"	float dist = distance(objSpaceCam.xz, position.xz); \n"
		"	alphaFade = (2.0f - (2.0f * dist / (fadeRange))); \n";
	}
	else if (mShader->wind == 2)
	{
		outStream <<
		"	float radiusCoeff = windParams.x; \n"
		"	float heightCoeff = windParams.y; \n"
		"	float factorX = windParams.z; \n"
		"	float factorY = windParams.w; \n";
		/* 
		2 different methods are used to for the sin calculation :
		- the first one gives a better effect but at the cost of a few fps because of the 2 sines
		- the second one uses less ressources but is a bit less realistic

			a sin approximation could be use to optimize performances
		*/
		
		// we can safely make permutations depending on "shaderQuality" since it does not change per material.
		//! might need to revisit this assumption when using more excessive caching (shaders that persist after settings change)
		if (mParent->getShaderQuality() > 0.6) outStream <<
			"	position.y += enableWind * sin(time + originPos.z + position.y + position.x) * radiusCoeff * radiusCoeff * factorY; \n"
			"	position.x += enableWind * sin(time + originPos.z ) * heightCoeff * heightCoeff * factorX ; \n";
		else outStream <<
			"	float sinval = enableWind * sin(time + originPos.z ); \n"
			"	position.y += sinval * radiusCoeff * radiusCoeff * factorY; \n"
			"	position.x += sinval * heightCoeff * heightCoeff * factorX ; \n";
	}
	
	outStream <<
	"	oPosition = mul(wvpMat, position); \n";
	if (vpNeedWMat()) outStream <<
	"	float4 worldPosition = mul(wMat, position); \n";
	
	if (fpNeedEyeVector()) outStream <<
		"	float3 eyeVector = worldPosition.xyz - eyePosition; \n" // transform eye into view space
		"	oTangentToCubeSpace0.xyzw = eyeVector.xxxx; \n"
		"	oTangentToCubeSpace1.xyzw = eyeVector.yyyy; \n"
		"	oTangentToCubeSpace2.xyzw = eyeVector.zzzz; \n";

	if (needNormalMap()) outStream <<
	"	float3 binormal = cross(tangent, normal); \n"	// calculate binormal
	"	float3x3 tbn = float3x3( tangent * bumpScale, \n"			// build TBN
	"										binormal * bumpScale, \n"
	"										normal ); \n"
	"	float3x3 tbnInv = transpose(tbn); \n" // transpose TBN to get inverse rotation to obj space
	"	float3x3 tmp = mul( (float3x3) wITMat, tbnInv ); \n" // concatenate with the upper-left 3x3 of inverse transpose world matrix 
	"	oTangentToCubeSpace0.xyz = tmp[0]; \n" // store in tangentToCubeSpace to pass this on to fragment shader
	"	oTangentToCubeSpace1.xyz = tmp[1]; \n"
	"	oTangentToCubeSpace2.xyz = tmp[2]; \n";
		
	if (needShadows())
	{
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream << "oLightPosition"+toStr(i)+" = mul(texWorldViewProjMatrix"+toStr(i)+", position); \n";
		}
	}
	
	std::string texCoordW = "1";
	if (needTerrainLightMap()) texCoordW = "worldPosition.z";
	
	std::string texCoordZ = "1";
	if (needShadows()) texCoordZ = "oPosition.z";
	
	outStream <<
	"	oTexCoord = float4(texCoord.x, texCoord.y, "+texCoordZ+", "+texCoordW+"); \n";
		
	outStream <<
	"	objectPos = position; \n";
	outStream <<
	"	objectPos.w = enableFog * saturate(fogParams.x * (oPosition.z - fogParams.y) * fogParams.w); \n"; // save fog amount in objectPos.w
	if (fpNeedWsNormal())
	{
		if(UsePerPixelNormals())
		{
			outStream <<	"	oNormal =  float4(normal,0) ; \n";
		}
		else
		{
			std::string wsNormalW = "1";
			if (needTerrainLightMap()) wsNormalW = "worldPosition.x";
			outStream <<
			"	oWsNormal = float4(mul( (float3x3) wITMat, normal ), "+wsNormalW+"); \n";
		}

	}

	if(MRTSupported())
	{
		if(!UsePerPixelNormals())
		{
			//view space normal 			
			outStream << " oViewNormal = mul(wvMat, float4(normal, 0)); \n";
		}
	}
	outStream <<
	"} \n";
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::vertexProgramParams(HighLevelGpuProgramPtr program)
{
	GpuProgramParametersSharedPtr params = program->getDefaultParameters();
	
	if (vpNeedWMat())
		params->setNamedAutoConstant("wMat", GpuProgramParameters::ACT_WORLD_MATRIX);
	params->setNamedAutoConstant("wvpMat", GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
	if (vpNeedWvMat())
		params->setNamedAutoConstant("wvMat", GpuProgramParameters::ACT_WORLDVIEW_MATRIX);
	if (fpNeedWsNormal())
		params->setNamedAutoConstant("wITMat", GpuProgramParameters::ACT_INVERSE_TRANSPOSE_WORLD_MATRIX);
	if (fpNeedEyeVector())
		params->setNamedAutoConstant("eyePosition", GpuProgramParameters::ACT_CAMERA_POSITION);
	
	params->setNamedAutoConstant("fogParams", GpuProgramParameters::ACT_FOG_PARAMS);
		
	if (mShader->wind == 1)
		params->setNamedAutoConstant("objSpaceCam", GpuProgramParameters::ACT_CAMERA_POSITION_OBJECT_SPACE);
	
	else if (mShader->wind == 2)
		params->setNamedConstant("enableWind", Real(1.0));
		
	individualVertexProgramParams(params);
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::individualVertexProgramParams(GpuProgramParametersSharedPtr params)
{
	if (needNormalMap())
		params->setNamedConstant("bumpScale", mDef->mProps->bumpScale);
	
	if (needShadows())
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
	{
		params->setNamedAutoConstant("texWorldViewProjMatrix"+toStr(i), GpuProgramParameters::ACT_TEXTURE_WORLDVIEWPROJ_MATRIX, i);
	}
	
	params->setNamedConstant("enableFog", mDef->mProps->fog ? Real(1.0) : Real(0.0));
}

//----------------------------------------------------------------------------------------

HighLevelGpuProgramPtr MaterialGenerator::createFragmentProgram()
{
	HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
	std::string progName = mDef->getName() + "_FP";

	HighLevelGpuProgramPtr ret = mgr.getByName(progName);
	if (!ret.isNull())
		mgr.remove(progName);

	ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
			"cg", GPT_FRAGMENT_PROGRAM);

	if(MRTSupported())
	{
		ret->setParameter("profiles", "ps_4_0 ps_3_0 fp40");
	}
	else
	{
		ret->setParameter("profiles", "ps_4_0 ps_2_x arbfp1");	
	}
	ret->setParameter("entry_point", "main_fp");

	StringUtil::StrStreamType sourceStr;
	generateFragmentProgramSource(sourceStr);
	ret->setSource(sourceStr.str());
	ret->load();
	fragmentProgramParams(ret);
	
	return ret;
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::fpRealtimeShadowHelperSource(Ogre::StringUtil::StrStreamType& outStream)
{
	/// shadow helper functions
	// 2x2 pcf
	outStream <<
	"float shadowPCF(sampler2D shadowMap, float4 shadowMapPos, float2 offset) \n"
	"{ \n"
	"	shadowMapPos = shadowMapPos / shadowMapPos.w; \n"
	"	float2 uv = shadowMapPos.xy; \n"
	"	float3 o = float3(offset, -offset.x) * 0.3f; \n";

	if(MRTSupported())
	{
		outStream <<
			"	float c =	(shadowMapPos.z <= tex2Dlod(shadowMap,  float4(uv.xy - o.xy,0,0)).r) ? 1 : 0; \n"
			"	c +=		(shadowMapPos.z <= tex2Dlod(shadowMap,  float4(uv.xy + o.xy,0,0)).r) ? 1 : 0; \n"
			"	c +=		(shadowMapPos.z <= tex2Dlod(shadowMap,  float4(uv.xy + o.zy,0,0)).r) ? 1 : 0; \n"
			"	c +=		(shadowMapPos.z <= tex2Dlod(shadowMap,  float4(uv.xy - o.zy,0,0)).r) ? 1 : 0; \n";
	}
	else
	{
		outStream <<
			"	float c =	(shadowMapPos.z <= tex2D(shadowMap, uv.xy - o.xy).r) ? 1 : 0; \n"
			"	c +=		(shadowMapPos.z <= tex2D(shadowMap, uv.xy + o.xy).r) ? 1 : 0; \n"
			"	c +=		(shadowMapPos.z <= tex2D(shadowMap, uv.xy + o.zy).r) ? 1 : 0; \n"
			"	c +=		(shadowMapPos.z <= tex2D(shadowMap, uv.xy - o.zy).r) ? 1 : 0; \n";
	}	
	
	outStream <<
		"	return c / 4;  \n"
		"} \n";
	
	// pssm
	outStream <<
	"float calcPSSMShadow(";
	
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
		outStream << "sampler2D shadowMap"+toStr(i)+",  \n";
	outStream << "\n";
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
		outStream << "float4 lsPos"+toStr(i)+",  \n";
	outStream << "\n";
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
		outStream << "float4 invShadowMapSize"+toStr(i)+",  \n";
	outStream << "\n";
	
	outStream <<
	"	float4 pssmSplitPoints, float camDepth \n"
	") \n"
	"{ \n"
	"	float shadow; \n";
	
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
	{

		if (i==0)
			outStream << "if (camDepth <= pssmSplitPoints.y) \n";
		else if (i < mParent->getNumShadowTex()-1)
			outStream << "else if (camDepth <= pssmSplitPoints."+getChannel(i+1)+") \n";
		else
			outStream << "else \n";
			
		outStream <<
		"{ \n"
		"	shadow = shadowPCF(shadowMap"+toStr(i)+", lsPos"+toStr(i)+", invShadowMapSize"+toStr(i)+".xy); \n"
		"} \n";
	}
	
	outStream <<
	"	return shadow; \n"
	"} \n";
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::fpShadowingParams(Ogre::StringUtil::StrStreamType& outStream)
{
	if (needShadows())
	{
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream <<
		"	uniform sampler2D shadowMap"+toStr(i)+" : TEXUNIT"+toStr(mShadowTexUnit_start+i)+", \n";
		}
		outStream << "\n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "in float4 lightPosition"+toStr(i)+" : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
		outStream << "\n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "uniform float4 invShadowMapSize"+toStr(i)+", \n";
		outStream << "\n";
		outStream << 
		"	uniform float4 pssmSplitPoints, \n";
	}
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::fpCalcShadowSource(Ogre::StringUtil::StrStreamType& outStream)
{
	if (needShadows() || needTerrainLightMap())
		outStream <<
		"	float shadowing; \n";
		
	if (needShadows())
	{
		outStream <<
		"	float shadowingRT;"
		"	shadowingRT = calcPSSMShadow(";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "shadowMap"+toStr(i)+", ";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "lightPosition"+toStr(i)+", ";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "invShadowMapSize"+toStr(i)+", ";
		
		outStream <<
		"pssmSplitPoints, texCoord.z); \n";
	}
	
	if (needTerrainLightMap())
	{
		outStream <<
		"	float shadowingLM; \n"
		"	float2 worldPos = float2(wsNormal.w, texCoord.w); \n" // get world position
		"	float2 lmTexCoord = (worldPos / terrainWorldSize) + 0.5; \n" // convert to image space 0..1
		"	shadowingLM = tex2D(terrainLightMap, lmTexCoord).x; \n" // fetch texture r channel
		"	if (enableTerrainLightMap == 0.f) shadowingLM = 1.f; \n";
	}
	
	// put together realtime and static shadow
	if (needShadows() || needTerrainLightMap())
	{
		if (needShadows() && needTerrainLightMap()) outStream <<
		"	shadowing = min(shadowingRT, shadowingLM); \n";
		else if (needShadows()) outStream <<
		"	shadowing = shadowingRT; \n";
		else /* if (needTerrainLightMap()) */ outStream <<
		"	shadowing = shadowingLM; \n";
	}
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::generateFragmentProgramSource(Ogre::StringUtil::StrStreamType& outStream)
{
	if (needShadows())
		fpRealtimeShadowHelperSource(outStream);
	mTexCoord_i=0;
	outStream <<
		"void main_fp("
		"	in float4 iPosition : POSITION, \n"
		"	in float4 position : COLOR, \n"
		"	in float4 texCoord : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
	
	if (mShader->vertexColour) outStream <<
		"	in float4 vertexColour : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
	
	if (mShader->wind == 1) outStream <<
		"	in float alphaFade : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
	
	if (fpNeedWsNormal()) 
	{
		if(UsePerPixelNormals())
		{
			outStream <<"	in float4 pNormal : COLOR1, \n";
		}
		else
		{
			if(MRTSupported())
			{
				outStream <<"	in float4 wsNormal : COLOR1, \n";
			}
			else
			{
				outStream <<"	in float4 wsNormal : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
			}
		}
	}
	if (fpNeedTangentToCube()) 
	{
		outStream << "	in float4 tangentToCubeSpace0 : TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n";
		outStream << "	in float4 tangentToCubeSpace1 : TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n";
		outStream << "	in float4 tangentToCubeSpace2 : TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n";
	}
	if (MRTSupported()) 
	{
		if(!UsePerPixelNormals())
		{
			outStream << "	in float4 viewNormal : TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n";
		}
	}

	if (vpNeedWvMat()) outStream <<
		"	uniform float4x4 wvMat, \n";
	if (fpNeedWMat()) outStream <<
		"	uniform float4x4 wMat, \n";

	if (needFresnel()) outStream <<
		"	uniform float fresnelBias, \n"
		"	uniform float fresnelScale, \n"
		"	uniform float fresnelPower, \n";
		
	if (needDiffuseMap()) outStream <<
		"	uniform sampler2D diffuseMap : TEXUNIT"+toStr(mDiffuseTexUnit)+", \n";
	
	if (needLightMap()) outStream <<
		"	uniform sampler2D lightMap : TEXUNIT"+toStr(mLightTexUnit)+", \n";
		
	if (needTerrainLightMap()) outStream <<
		"	uniform sampler2D terrainLightMap : TEXUNIT"+toStr(mTerrainLightTexUnit)+", \n"
		"	uniform float enableTerrainLightMap, \n"
		"	uniform float terrainWorldSize, \n";

	if (needBlendMap()) outStream <<
		"	uniform sampler2D blendMap : TEXUNIT"+toStr(mBlendTexUnit)+", \n";
		
	if (needAlphaMap()) outStream <<
		"	uniform sampler2D alphaMap	 : TEXUNIT"+toStr(mAlphaTexUnit)+", \n";
		
	if (needNormalMap()) outStream <<
		"	uniform sampler2D normalMap  : TEXUNIT"+toStr(mNormalTexUnit)+", \n";
		
	if (needEnvMap()) outStream << 
		"	uniform samplerCUBE envMap : TEXUNIT"+toStr(mEnvTexUnit)+", \n";
	if (needEnvMap() && !needFresnel()) outStream << 
		"	uniform float reflAmount, \n";
		
	if (needSpecMap()) outStream <<
		"	uniform sampler2D specMap : TEXUNIT"+toStr(mSpecTexUnit)+", \n";
		
	if (needReflectivityMap()) outStream <<
		"	uniform sampler2D reflectivityMap : TEXUNIT"+toStr(mReflTexUnit)+", \n";
	
	fpShadowingParams(outStream);
		
	// lighting params
	// only 1 directional light is supported
	if (fpNeedLighting())
	{
		outStream <<
		// light
		"	uniform float3 lightDiffuse, \n"
		"	uniform float3 lightSpecular, \n"
		"	uniform float4 lightPosition, \n"
		"	uniform float3 globalAmbient, \n"
		// material
		"	uniform float3 matAmbient, \n"
		"	uniform float3 matDiffuse, \n";
		if (!needSpecMap()) outStream <<
		"	uniform float4 matSpecular, \n"; // shininess in w
	}
	outStream <<
		"	uniform float3 fogColor, \n";
		
	if (mDef->mProps->transparent && needLightingAlpha())
		outStream <<
		"	uniform float4 lightingAlpha, \n";
		
	if (mDef->mProps->transparent) outStream <<
		"	uniform float alphaRejectValue, \n";
	
	
	if(MRTSupported())
	{
		outStream << "	out float4 oColor : COLOR0, \n";
		outStream << "	out float4 oColor1 : COLOR1, \n";
		outStream << "	uniform float far \n";
	}
	else
	{
			outStream << "	out float4 oColor : COLOR \n";
	}
	outStream << 	") \n"
		"{ \n";
	
	// calc shadowing
	fpCalcShadowSource(outStream);
	
	if (fpNeedEyeVector()) outStream <<
		"	float3 eyeVector = normalize(float3(tangentToCubeSpace0.w, tangentToCubeSpace1.w, tangentToCubeSpace2.w)); \n";
	if (fpNeedWsNormal())
	{
		outStream <<
		"	float3 normal;";
		if (needNormalMap()) outStream <<
			"	float4 tsNormal = (tex2D( normalMap, texCoord.xy) * 2.0 - 1.0 ); \n" // fetch tangent space normal and decompress from range-compressed
			"	normal.x = dot( tangentToCubeSpace0.xyz, tsNormal.xyz ); \n"
			"	normal.y = dot( tangentToCubeSpace1.xyz, tsNormal.xyz ); \n"
			"	normal.z = dot( tangentToCubeSpace2.xyz, tsNormal.xyz ); \n";
		else outStream <<
			"	normal = wsNormal; \n";
		
		outStream << 
		"	normal = normalize(normal); \n";
	}
	
	// fetch diffuse texture
	if (needDiffuseMap()) outStream <<
		"	float4 diffuseTex = tex2D(diffuseMap, texCoord.xy); \n";
	
	else if (needLightMap() && needBlendMap())
	{
		outStream <<
		"	float3 lightTex = tex2D(lightMap, texCoord.xy).x * matDiffuse.xyz; \n"
		"	float4 blendTex = tex2D(blendMap, texCoord.xy); \n"
		"	float4 diffuseTex = float4( lerp(lightTex.xyz, blendTex.xyz, blendTex.a), 1 ); \n";
	}

	// calculate lighting (per-pixel)
	if (fpNeedLighting())
	{
		outStream <<	
		// compute the diffuse term
		"	float3 lightDir = normalize(lightPosition.xyz - (position.xyz * lightPosition.w)); \n"
		"	float diffuseLight = max(dot(lightDir, normal), 0); \n";
		
		if ((needLightMap() && needBlendMap())) outStream <<
			"	float3 diffuse = lightDiffuse.xyz *  diffuseLight * diffuseTex.xyz; \n";
		else if (needDiffuseMap()) outStream <<
			"	float3 diffuse = matDiffuse.xyz * lightDiffuse.xyz *  diffuseLight * diffuseTex.xyz; \n";
		else outStream <<
			"	float3 diffuse = matDiffuse.xyz * lightDiffuse.xyz * diffuseLight; \n";
			
		// compute the specular term
		if (needSpecMap()) outStream <<
			"	float4 specTex = tex2D(specMap, texCoord.xy); \n"
			"	float3 matSpec = specTex.xyz; \n"
			"	float shininess = specTex.w*255; \n";
		else outStream <<
			"	float3 matSpec = matSpecular.xyz; \n"
			"	float shininess = matSpecular.w; \n";
		outStream <<
		"	float3 viewVec = -eyeVector; \n"
		"	float3 half = normalize(lightDir + viewVec); \n"
		"	float specularLight = pow(max(dot(normal, half), 0), shininess); \n"
		"	if (matSpec.x == 0 && matSpec.y == 0 && matSpec.z == 0) specularLight = 0; \n"
		"	if (diffuseLight <= 0) specularLight = 0; \n"
		"	float3 specular = matSpec.xyz * lightSpecular.xyz * specularLight; \n";

		// compute the ambient term
		outStream << "	float3 ambient = matAmbient.xyz * globalAmbient.xyz ";
		if (needDiffuseMap() || (needLightMap() && needBlendMap())) outStream <<	"* diffuseTex.xyz";
		outStream << "; \n";

		// add all terms together (also with shadow)
		if (needShadows() || needTerrainLightMap()) outStream <<
		"	float3 lightColour = ambient + diffuse*shadowing + specular*shadowing; \n";
		else outStream <<
		"	float3 lightColour = ambient + diffuse + specular; \n";
	}
	
	// cube reflection
	if (needEnvMap())
	{
		if (needFresnel())
		{
			outStream <<
			"	float facing = 1.0 - max(abs(dot(eyeVector, normal)), 0); \n";
			if (!needReflectivityMap()) outStream <<
				"	float reflectionFactor = saturate(fresnelBias + fresnelScale * pow(facing, fresnelPower)); \n";
			else outStream <<
				"	float reflectionFactor = tex2D(reflectivityMap, texCoord.xy).r * saturate(fresnelBias + fresnelScale * pow(facing, fresnelPower)); \n";
		}
		else
		{
			if (!needReflectivityMap()) outStream <<
				"	float reflectionFactor = reflAmount; \n";
			else outStream <<
				"	float reflectionFactor = tex2D(reflectivityMap, texCoord.xy).r * reflAmount; \n";
		}
		outStream << 
		"	float3 r = reflect( eyeVector, normal ); \n" // calculate reflection vector
		"	float4 envColor = texCUBE(envMap, r); \n"; // fetch cube map

		if (fpNeedLighting()) outStream <<
		"	float4 color1 = lerp(float4(lightColour,1), envColor, reflectionFactor); \n";
		else outStream <<
		"	float4 color1 = lerp(diffuseColour, envColor, reflectionFactor); \n";
	}
	else
	{
		if (fpNeedLighting()) outStream <<
		"	float4 color1 = float4(lightColour,1); \n";
		else if (needShadows() || needTerrainLightMap()) outStream << // shadows, but no lighting
		"	float4 color1 = diffuseTex * (0.65f + 0.35f * shadowing); \n";
		else outStream <<
		"	float4 color1 = diffuseTex; \n";
	}
	
	if (mShader->vertexColour) outStream <<
		"	color1 *= vertexColour; \n";
	
	// add fog
	outStream <<
		"	oColor = lerp(color1, float4(fogColor,1), position.w); \n";
	
	// debug colour output  ------------------------------------------
	
	// world position (for lightmap)
	//if (needTerrainLightMap()) outStream <<
	//	"	oColor = oColor*float4(texCoord.w, wsNormal.w, 1, 1); \n";
	
	// normal
	//outStream <<
	//"	oColor = oColor * float4(normal.x, normal.y, normal.z, 1); \n";
	
	// spec
	//outStream <<
	//"	oColor = oColor * float4(specularLight, 0.0, 0.0, 1.0); \n";
	
	// ---------------------------------------------------------------
		
	// alpha
	if (mDef->mProps->transparent)
	{
		if (needAlphaMap()) outStream <<
			"	float alpha = tex2D(alphaMap, texCoord.xy).r; \n"; // use only r channel
		else if (needLightingAlpha())
		{
			if (mDef->mProps->lightingAlpha.w != 0 && needDiffuseMap()) outStream <<
				"	float alpha = lightingAlpha.x + lightingAlpha.y * diffuseLight + lightingAlpha.z * specularLight + (1-diffuseTex.r)*lightingAlpha.w; \n";
			else outStream <<
				"	float alpha = lightingAlpha.x + lightingAlpha.y * diffuseLight + lightingAlpha.z * specularLight; \n";
		}
		else
		{
			if (needDiffuseMap()) outStream <<
				"	float alpha = diffuseTex.a; \n";
			else {
				outStream <<
				"	float alpha = 1.0; \n"; // no way to get alpha value, we don't have diffuse tex and user didnt supply lightingAlpha
				LogO("[MaterialFactory] WARNING: Material '"+mDef->getName()+"' declared as transparent, but no way to get alpha value.");
			}
		}
		
		// discard rejected alpha pixels
		outStream << 
			"	clip( alpha<alphaRejectValue ? -1:1); \n";
		
		if (mShader->wind == 1) outStream <<
			"	alpha *= alphaFade; \n";
		
		outStream << 
		"	oColor.w = alpha; \n";
	}
	
	if(MRTSupported())
	{
		outStream <<  "float4 viewPosition = mul(wvMat, float4(position.xyz,1.0)); \n";
		if(UsePerPixelNormals())
		{
			outStream <<  "float4 viewNormal = mul(wvMat, pNormal); \n";
		}
		outStream <<  "oColor1 = float4(length(viewPosition.xyz) / far, normalize(viewNormal.xyz).xyz); \n";
		if(mDef->mProps->transparent)
		{
			// mutiply the diffuse texture alpha
			outStream << "oColor1 = oColor1 * tex2D(diffuseMap, texCoord.xy).a;";    
		}
	}
	outStream << 
		"} \n";
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::fragmentProgramParams(HighLevelGpuProgramPtr program)
{
	GpuProgramParametersSharedPtr params = program->getDefaultParameters();

	if (fpNeedLighting())
	{
		params->setNamedAutoConstant("lightDiffuse", GpuProgramParameters::ACT_LIGHT_DIFFUSE_COLOUR, 0);
		params->setNamedAutoConstant("lightPosition", GpuProgramParameters::ACT_LIGHT_POSITION, 0);
		params->setNamedAutoConstant("matAmbient", GpuProgramParameters::ACT_SURFACE_AMBIENT_COLOUR);
		params->setNamedAutoConstant("matDiffuse", GpuProgramParameters::ACT_SURFACE_DIFFUSE_COLOUR);
		if (!needSpecMap())
			params->setNamedAutoConstant("matSpecular", GpuProgramParameters::ACT_SURFACE_SPECULAR_COLOUR);
		params->setNamedAutoConstant("globalAmbient", GpuProgramParameters::ACT_AMBIENT_LIGHT_COLOUR);
	}
	
	params->setNamedAutoConstant("fogColor", GpuProgramParameters::ACT_FOG_COLOUR);
	
	if (vpNeedWvMat())
		params->setNamedAutoConstant("wvMat", GpuProgramParameters::ACT_WORLDVIEW_MATRIX);

	if(MRTSupported())
	{
		params->setNamedAutoConstant("far", GpuProgramParameters::ACT_FAR_CLIP_DISTANCE);
	}
		
	if (needTerrainLightMap())
		params->setNamedConstant("enableTerrainLightMap", Real(1));
	
	individualFragmentProgramParams(params);
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::individualFragmentProgramParams(Ogre::GpuProgramParametersSharedPtr params)
{
	if (needEnvMap() && !needFresnel())
	{
		params->setNamedConstant("reflAmount", mDef->mProps->reflAmount);
	}
	if (needFresnel())
	{
		params->setNamedConstant("fresnelScale", mDef->mProps->fresnelScale);
		params->setNamedConstant("fresnelBias", mDef->mProps->fresnelBias);
		params->setNamedConstant("fresnelPower", mDef->mProps->fresnelPower);
	}
	
	if (mDef->mProps->transparent)
		params->setNamedConstant("alphaRejectValue", Real(float(mDef->mProps->alphaRejectValue)/float(256.0f)));

	if (needLightingAlpha())
		params->setNamedConstant("lightingAlpha", mDef->mProps->lightingAlpha);
		
	if (fpNeedLighting())
		params->setNamedConstant("lightSpecular", mDef->mProps->specular);
	
	if (needShadows())
	{
		params->setNamedConstant("pssmSplitPoints", mParent->pApp->splitPoints);
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			params->setNamedAutoConstant("invShadowMapSize"+toStr(i), GpuProgramParameters::ACT_INVERSE_TEXTURE_SIZE, i+mShadowTexUnit_start);
	}
	
	if (needTerrainLightMap())
		params->setNamedConstant("terrainWorldSize", Real(1025)); // real value set later in changeShadows()
}

