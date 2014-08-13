/*
 OFX Noise plugin.

 Copyright (C) 2014 INRIA

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 INRIA
 Domaine de Voluceau
 Rocquencourt - B.P. 105
 78153 Le Chesnay Cedex - France

 The skeleton for this source file is from:
 OFX Genereator example plugin, a plugin that illustrates the use of the OFX Support library.

 Copyright (C) 2004-2005 The Open Effects Association Ltd
 Author Bruno Nicoletti bruno@thefoundry.co.uk

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * Neither the name The Open Effects Association Ltd, nor the names of its
 contributors may be used to endorse or promote products derived from this
 software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The Open Effects Association Ltd
 1 Wardour St
 London W1D 6PA
 England


 */

#include <limits>
#include <cmath>
#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"

#include "ofxsProcessing.H"
#include "ofxsMacros.h"

#include "randomGenerator.H"

#define kPluginName "NoiseOFX"
#define kPluginGrouping "Draw"
#define kPluginDescription "Generate noise."
#define kPluginIdentifier "net.sf.openfx:Noise"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kNoiseParamName "noise"
#define kNoiseParamLabel "Noise"
#define kNoiseParamHint "How much noise to make."

#define kSeedParamName "seed"
#define kSeedParamLabel "Seed"
#define kSeedParamHint "Random seed: change this if you want different instances to have different noise."

using namespace OFX;

////////////////////////////////////////////////////////////////////////////////
// base class for the noise

/** @brief  Base class used to blend two images together */
class NoiseGeneratorBase : public OFX::ImageProcessor
{
protected:
    float       _noiseLevel;   // noise amplitude
    float       _mean;   // mean value
    uint32_t    _seed;    // base seed
public:
    /** @brief no arg ctor */
    NoiseGeneratorBase(OFX::ImageEffect &instance)
    : OFX::ImageProcessor(instance)
    , _noiseLevel(0.5f)
    , _seed(0)
    {
    }

    /** @brief set the scale */
    void setNoiseLevel(float v) {_noiseLevel = v;}

    /** @brief set the offset */
    void setNoiseMean(float v) {_mean = v;}

    /** @brief the seed to use */
    void setSeed(uint32_t v) {_seed = v;}
};

static unsigned int hash(unsigned int a)
{
    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a;
}

/** @brief templated class to blend between two images */
template <class PIX, int nComponents, int max>
class NoiseGenerator : public NoiseGeneratorBase
{
public:
    // ctor
    NoiseGenerator(OFX::ImageEffect &instance)
    : NoiseGeneratorBase(instance)
    {}

    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float noiseLevel = _noiseLevel;

        // set up a random number generator and set the seed
        RandomGenerator randy;;

        // push pixels
        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if (_effect.abort()) {
                break;
            }
            
            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                // for a given x,y position, the output should always be the same.
                randy.reseed(hash(x + 0x10000*_seed) + y);
                for (int c = 0; c < nComponents; c++) {
                    // get the random value out of it, scale up by the pixel max level and the noise level
                    double randValue = _mean + max * noiseLevel * (randy.random()-0.5);

                    if (max == 1) // implies floating point, so don't clamp
                        dstPix[c] = PIX(randValue);
                    else {  // integer base one, clamp it
                        dstPix[c] = randValue < 0 ? 0 : (randValue > max ? max : PIX(randValue));
                    }
                }
                dstPix += nComponents;
            }
        }
    }

};

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class NoisePlugin : public OFX::ImageEffect
{
protected:
    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *srcClip_;
    OFX::Clip *dstClip_;

    OFX::DoubleParam  *noise_;
    OFX::IntParam  *seed_;

public:
    /** @brief ctor */
    NoisePlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , dstClip_(0)
    , noise_(0)
    , seed_(0)
    {
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
        assert(dstClip_ && (dstClip_->getPixelComponents() == ePixelComponentRGB || dstClip_->getPixelComponents() == ePixelComponentRGBA || dstClip_->getPixelComponents() == ePixelComponentAlpha));
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
        assert(srcClip_ && (srcClip_->getPixelComponents() == ePixelComponentRGB || srcClip_->getPixelComponents() == ePixelComponentRGBA || srcClip_->getPixelComponents() == ePixelComponentAlpha));
        noise_   = fetchDoubleParam(kNoiseParamName);
        seed_   = fetchIntParam(kSeedParamName);
        assert(noise_ && seed_);
    }

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    /* Override the clip preferences, we need to say we are setting the frame varying flag */
    virtual void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;

    /* set up and run a processor */
    void setupAndProcess(NoiseGeneratorBase &, const OFX::RenderArguments &args);

    /** @brief The get RoD action.  We flag an infinite rod */
    bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
};


////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from


/* set up and run a processor */
void
NoisePlugin::setupAndProcess(NoiseGeneratorBase &processor, const OFX::RenderArguments &args)
{
    // get a dst image
    std::auto_ptr<OFX::Image>  dst(dstClip_->fetchImage(args.time));
    //OFX::BitDepthEnum         dstBitDepth    = dst->getPixelDepth();
    //OFX::PixelComponentEnum   dstComponents  = dst->getPixelComponents();

    // set the images
    processor.setDstImg(dst.get());

    // set the render window
    processor.setRenderWindow(args.renderWindow);

    // set the scales
    // noise level depends on the render scale
    // (the following formula is for Gaussian noise only, but we use it as an approximation)
    double noise = noise_->getValueAtTime(args.time);
    processor.setNoiseLevel((float)(noise * std::sqrt(args.renderScale.x * args.renderScale.y)));
    processor.setNoiseMean((float)(noise / 2.));

    // set the seed based on the current time, and double it we get difference seeds on different fields
    processor.setSeed(uint32_t(args.time * 2.0f + seed_->getValueAtTime(args.time)));

    // Call the base class process member, this will call the derived templated process code
    processor.process();
}

/* Override the clip preferences, we need to say we are setting the frame varying flag */
void
NoisePlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    clipPreferences.setOutputFrameVarying(true);
}

/** @brief The get RoD action.  We flag an infinite rod */
bool
NoisePlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &/*args*/, OfxRectD &rod)
{
    // we can generate noise anywhere on the image plan, so set our RoD to be infinite
    rod.x1 = rod.y1 = kOfxFlagInfiniteMin;
    rod.x2 = rod.y2 = kOfxFlagInfiniteMax;
    return true;
}

// the overridden render function
void
NoisePlugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum       dstBitDepth    = dstClip_->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = dstClip_->getPixelComponents();

    // do the rendering
    if (dstComponents == OFX::ePixelComponentRGBA) {
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte: {
                NoiseGenerator<unsigned char, 4, 255> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthUShort: {
                NoiseGenerator<unsigned short, 4, 65535> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthFloat: {
                NoiseGenerator<float, 4, 1> fred(*this);
                setupAndProcess(fred, args);
            }
                break;
            default :
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    } else {
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte: {
                NoiseGenerator<unsigned char, 1, 255> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthUShort: {
                NoiseGenerator<unsigned short, 1, 65535> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthFloat: {
                NoiseGenerator<float, 1, 1> fred(*this);
                setupAndProcess(fred, args);
            }
                break;
            default:
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    }
}

mDeclarePluginFactory(NoisePluginFactory, {}, {});

using namespace OFX;

void NoisePluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    // basic labels
    desc.setLabels(kPluginName, kPluginName, kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    desc.addSupportedContext(eContextGenerator);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(true);
    desc.setSupportsTiles(true);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);
    desc.setRenderTwiceAlways(false);
}

void NoisePluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, ContextEnum /*context*/)
{
    // there has to be an input clip, even for generators
    ClipDescriptor* srcClip = desc.defineClip( kOfxImageEffectSimpleSourceClipName );
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setSupportsTiles(true);
    srcClip->setOptional(true);

    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(true);
    dstClip->setFieldExtraction(eFieldExtractSingle);

    PageParamDescriptor *page = desc.definePageParam("Controls");

    DoubleParamDescriptor *noise = desc.defineDoubleParam(kNoiseParamName);
    noise->setLabels(kNoiseParamLabel, kNoiseParamLabel, kNoiseParamLabel);
    noise->setHint(kNoiseParamHint);
    noise->setDefault(0.2);
    noise->setRange(0, 10);
    noise->setIncrement(0.1);
    noise->setDisplayRange(0, 1);
    noise->setAnimates(true); // can animate
    noise->setDoubleType(eDoubleTypeScale);
    page->addChild(*noise);

    IntParamDescriptor *seed = desc.defineIntParam(kSeedParamName);
    seed->setLabels(kSeedParamName, kSeedParamName, kSeedParamLabel);
    seed->setHint(kSeedParamHint);
    seed->setDefault(2000);
    seed->setAnimates(true); // can animate
    page->addChild(*seed);
}

ImageEffect* NoisePluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new NoisePlugin(handle);
}

void getNoisePluginID(OFX::PluginFactoryArray &ids)
{
    static NoisePluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
    ids.push_back(&p);
}
