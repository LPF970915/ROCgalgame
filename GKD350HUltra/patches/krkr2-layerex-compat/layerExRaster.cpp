// layerExRaster — Layer raster scroll copy effect
// Ported from https://github.com/wamsoft/layerExRaster

#include "ncbind.hpp"
#include <algorithm>
#include <cstring>
#include <vector>
#define _USE_MATH_DEFINES
#include <cmath>

#define NCB_MODULE_NAME TJS_W("layerExRaster.dll")

#include "layerExBase_wamsoft.hpp"

struct layerExRaster : public layerExBase
{
    layerExRaster(DispatchT obj) : layerExBase(obj) {}

    void copyRaster(tTJSVariant layer, int maxh, int lines, int cycle, tjs_int64 time) {
        iTJSDispatch2 *layerobj = layer.AsObjectNoAddRef();
        if (!layerobj) {
            TVPThrowExceptionMessage(TJS_W("copyRaster source must be Layer."));
            return;
        }

        tjs_int width = 0, height = 0, pitch = 0;
        unsigned char *buffer = nullptr;
        tTJSVariant var;
        if (TJS_FAILED(layerobj->PropGet(0, TJS_W("imageWidth"), nullptr, &var, layerobj)))
            return;
        width = (tjs_int)var;
        if (TJS_FAILED(layerobj->PropGet(0, TJS_W("imageHeight"), nullptr, &var, layerobj)))
            return;
        height = (tjs_int)var;
        if (TJS_FAILED(layerobj->PropGet(0, TJS_W("mainImageBuffer"), nullptr, &var, layerobj)))
            return;
        buffer = reinterpret_cast<unsigned char *>((tTVInteger)var);
        if (TJS_FAILED(layerobj->PropGet(0, TJS_W("mainImageBufferPitch"), nullptr, &var, layerobj)))
            return;
        pitch = (tjs_int)var;

        if (_width != width || _height != height || !_buffer || !buffer ||
            _pitch <= 0 || pitch <= 0 || _clipLeft < 0 || _clipTop < 0 ||
            _clipWidth <= 0 || _clipHeight <= 0 ||
            _clipLeft + _clipWidth > _width || _clipTop + _clipHeight > _height ||
            pitch < width * 4 || _pitch < _width * 4 || lines <= 0 || cycle == 0)
            return;

        const double omega = 2 * M_PI / lines;
        const tjs_int curH = std::max<tjs_int>(0, (tjs_int)maxh);
        double rad = -omega * (double)time / (double)cycle * (height / 2);
        const unsigned char *srcBase = buffer + pitch * _clipTop + _clipLeft * 4;
        unsigned char *dstBase = _buffer + _pitch * _clipTop + _clipLeft * 4;
        std::vector<tjs_uint32> row((size_t)_clipWidth);

        for (tjs_int n = 0; n < _clipHeight; n++, rad += omega) {
            // Source and destination are commonly the same Layer. Snapshot the
            // row before shifting so overlap cannot corrupt the next read.
            const tjs_uint32 *src = reinterpret_cast<const tjs_uint32 *>(srcBase + n * pitch);
            tjs_uint32 *dest = reinterpret_cast<tjs_uint32 *>(dstBase + n * _pitch);
            std::memcpy(row.data(), src, (size_t)_clipWidth * sizeof(tjs_uint32));
            const tjs_int d = (tjs_int)(sin(rad) * curH);
            if (d >= 0) {
                if (d < _clipWidth)
                    std::copy(row.begin(), row.begin() + (_clipWidth - d), dest + d);
            } else if (-d < _clipWidth) {
                std::copy(row.begin() - d, row.end(), dest);
            }
        }
        redraw();
    }
};

static tjs_error copyRasterCompat(tTJSVariant *result, tjs_int numparams,
                                  tTJSVariant **param,
                                  iTJSDispatch2 *objthis) {
    if (result)
        result->Clear();
    if (!objthis)
        return TJS_E_INVALIDOBJECT;
    if (numparams < 5)
        return TJS_E_BADPARAMCOUNT;
    for (tjs_int index = 0; index < 5; ++index) {
        if (!param[index] || param[index]->Type() == tvtVoid)
            return TJS_E_INVALIDPARAM;
    }
    if (param[0]->Type() != tvtObject || !param[0]->AsObjectNoAddRef())
        return TJS_E_INVALIDPARAM;

    layerExRaster *obj =
        ncbInstanceAdaptor<layerExRaster>::GetNativeInstance(objthis);
    if (!obj) {
        obj = new layerExRaster(objthis);
        if (!ncbInstanceAdaptor<layerExRaster>::SetAdaptorWithNativeInstance(
                objthis, obj)) {
            delete obj;
            return TJS_E_NATIVECLASSCRASH;
        }
    }

    obj->reset();
    obj->copyRaster(*param[0], (tjs_int)(tTVInteger)*param[1],
                    (tjs_int)(tTVInteger)*param[2],
                    (tjs_int)(tTVInteger)*param[3],
                    (tjs_int64)(tTVInteger)*param[4]);
    return TJS_S_OK;
}

NCB_GET_INSTANCE_HOOK(layerExRaster)
{
    NCB_INSTANCE_GETTER(objthis) {
        ClassT* obj = GetNativeInstance(objthis);
        if (!obj) {
            obj = new ClassT(objthis);
            SetNativeInstance(objthis, obj);
        }
        obj->reset();
        return obj;
    }
    ~NCB_GET_INSTANCE_HOOK_CLASS() {}
};

NCB_ATTACH_CLASS_WITH_HOOK(layerExRaster, Layer) {
    NCB_METHOD_RAW_CALLBACK(copyRaster, copyRasterCompat, 0);
}
