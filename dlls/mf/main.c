/*
 * Copyright (C) 2015 Austin English
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "mfidl.h"
#include "rpcproxy.h"

#include "initguid.h"
#include "mf.h"

#undef INITGUID
#include <guiddef.h>
#include "mfapi.h"
#include "mferror.h"

#include "mf_private.h"

#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

static HINSTANCE mf_instance;

struct activate_object
{
    IMFActivate IMFActivate_iface;
    LONG refcount;
    IMFAttributes *attributes;
    IUnknown *object;
    const struct activate_funcs *funcs;
    void *context;
};

static struct activate_object *impl_from_IMFActivate(IMFActivate *iface)
{
    return CONTAINING_RECORD(iface, struct activate_object, IMFActivate_iface);
}

static HRESULT WINAPI activate_object_QueryInterface(IMFActivate *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFActivate) ||
            IsEqualIID(riid, &IID_IMFAttributes) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFActivate_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI activate_object_AddRef(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    ULONG refcount = InterlockedIncrement(&activate->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI activate_object_Release(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    ULONG refcount = InterlockedDecrement(&activate->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        activate->funcs->free_private(activate->context);
        if (activate->object)
            IUnknown_Release(activate->object);
        IMFAttributes_Release(activate->attributes);
        heap_free(activate);
    }

    return refcount;
}

static HRESULT WINAPI activate_object_GetItem(IMFActivate *iface, REFGUID key, PROPVARIANT *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetItem(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetItemType(IMFActivate *iface, REFGUID key, MF_ATTRIBUTE_TYPE *type)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), type);

    return IMFAttributes_GetItemType(activate->attributes, key, type);
}

static HRESULT WINAPI activate_object_CompareItem(IMFActivate *iface, REFGUID key, REFPROPVARIANT value, BOOL *result)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %p.\n", iface, debugstr_guid(key), value, result);

    return IMFAttributes_CompareItem(activate->attributes, key, value, result);
}

static HRESULT WINAPI activate_object_Compare(IMFActivate *iface, IMFAttributes *theirs, MF_ATTRIBUTES_MATCH_TYPE type,
        BOOL *result)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %p, %d, %p.\n", iface, theirs, type, result);

    return IMFAttributes_Compare(activate->attributes, theirs, type, result);
}

static HRESULT WINAPI activate_object_GetUINT32(IMFActivate *iface, REFGUID key, UINT32 *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetUINT32(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetUINT64(IMFActivate *iface, REFGUID key, UINT64 *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetUINT64(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetDouble(IMFActivate *iface, REFGUID key, double *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetDouble(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetGUID(IMFActivate *iface, REFGUID key, GUID *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetGUID(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetStringLength(IMFActivate *iface, REFGUID key, UINT32 *length)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), length);

    return IMFAttributes_GetStringLength(activate->attributes, key, length);
}

static HRESULT WINAPI activate_object_GetString(IMFActivate *iface, REFGUID key, WCHAR *value,
        UINT32 size, UINT32 *length)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %d, %p.\n", iface, debugstr_guid(key), value, size, length);

    return IMFAttributes_GetString(activate->attributes, key, value, size, length);
}

static HRESULT WINAPI activate_object_GetAllocatedString(IMFActivate *iface, REFGUID key,
        WCHAR **value, UINT32 *length)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %p.\n", iface, debugstr_guid(key), value, length);

    return IMFAttributes_GetAllocatedString(activate->attributes, key, value, length);
}

static HRESULT WINAPI activate_object_GetBlobSize(IMFActivate *iface, REFGUID key, UINT32 *size)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), size);

    return IMFAttributes_GetBlobSize(activate->attributes, key, size);
}

static HRESULT WINAPI activate_object_GetBlob(IMFActivate *iface, REFGUID key, UINT8 *buf,
        UINT32 bufsize, UINT32 *blobsize)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %d, %p.\n", iface, debugstr_guid(key), buf, bufsize, blobsize);

    return IMFAttributes_GetBlob(activate->attributes, key, buf, bufsize, blobsize);
}

static HRESULT WINAPI activate_object_GetAllocatedBlob(IMFActivate *iface, REFGUID key, UINT8 **buf, UINT32 *size)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %p.\n", iface, debugstr_guid(key), buf, size);

    return IMFAttributes_GetAllocatedBlob(activate->attributes, key, buf, size);
}

static HRESULT WINAPI activate_object_GetUnknown(IMFActivate *iface, REFGUID key, REFIID riid, void **ppv)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s, %p.\n", iface, debugstr_guid(key), debugstr_guid(riid), ppv);

    return IMFAttributes_GetUnknown(activate->attributes, key, riid, ppv);
}

static HRESULT WINAPI activate_object_SetItem(IMFActivate *iface, REFGUID key, REFPROPVARIANT value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_SetItem(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_DeleteItem(IMFActivate *iface, REFGUID key)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s.\n", iface, debugstr_guid(key));

    return IMFAttributes_DeleteItem(activate->attributes, key);
}

static HRESULT WINAPI activate_object_DeleteAllItems(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p.\n", iface);

    return IMFAttributes_DeleteAllItems(activate->attributes);
}

static HRESULT WINAPI activate_object_SetUINT32(IMFActivate *iface, REFGUID key, UINT32 value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %d.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_SetUINT32(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetUINT64(IMFActivate *iface, REFGUID key, UINT64 value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s.\n", iface, debugstr_guid(key), wine_dbgstr_longlong(value));

    return IMFAttributes_SetUINT64(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetDouble(IMFActivate *iface, REFGUID key, double value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %f.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_SetDouble(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetGUID(IMFActivate *iface, REFGUID key, REFGUID value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s.\n", iface, debugstr_guid(key), debugstr_guid(value));

    return IMFAttributes_SetGUID(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetString(IMFActivate *iface, REFGUID key, const WCHAR *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s.\n", iface, debugstr_guid(key), debugstr_w(value));

    return IMFAttributes_SetString(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetBlob(IMFActivate *iface, REFGUID key, const UINT8 *buf, UINT32 size)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %d.\n", iface, debugstr_guid(key), buf, size);

    return IMFAttributes_SetBlob(activate->attributes, key, buf, size);
}

static HRESULT WINAPI activate_object_SetUnknown(IMFActivate *iface, REFGUID key, IUnknown *unknown)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("(%p)->(%s, %p)\n", iface, debugstr_guid(key), unknown);

    return IMFAttributes_SetUnknown(activate->attributes, key, unknown);
}

static HRESULT WINAPI activate_object_LockStore(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p.\n", iface);

    return IMFAttributes_LockStore(activate->attributes);
}

static HRESULT WINAPI activate_object_UnlockStore(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p.\n", iface);

    return IMFAttributes_UnlockStore(activate->attributes);
}

static HRESULT WINAPI activate_object_GetCount(IMFActivate *iface, UINT32 *count)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %p.\n", iface, count);

    return IMFAttributes_GetCount(activate->attributes, count);
}

static HRESULT WINAPI activate_object_GetItemByIndex(IMFActivate *iface, UINT32 index, GUID *key, PROPVARIANT *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %u, %p, %p.\n", iface, index, key, value);

    return IMFAttributes_GetItemByIndex(activate->attributes, index, key, value);
}

static HRESULT WINAPI activate_object_CopyAllItems(IMFActivate *iface, IMFAttributes *dest)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %p.\n", iface, dest);

    return IMFAttributes_CopyAllItems(activate->attributes, dest);
}

static HRESULT WINAPI activate_object_ActivateObject(IMFActivate *iface, REFIID riid, void **obj)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    IUnknown *object;
    HRESULT hr;

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (!activate->object)
    {
        if (FAILED(hr = activate->funcs->create_object((IMFAttributes *)iface, activate->context, &object)))
            return hr;

        if (InterlockedCompareExchangePointer((void **)&activate->object, object, NULL))
            IUnknown_Release(object);
    }

    return IUnknown_QueryInterface(activate->object, riid, obj);
}

static HRESULT WINAPI activate_object_ShutdownObject(IMFActivate *iface)
{
    FIXME("%p.\n", iface);

    return E_NOTIMPL;
}

static HRESULT WINAPI activate_object_DetachObject(IMFActivate *iface)
{
    FIXME("%p.\n", iface);

    return E_NOTIMPL;
}

static const IMFActivateVtbl activate_object_vtbl =
{
    activate_object_QueryInterface,
    activate_object_AddRef,
    activate_object_Release,
    activate_object_GetItem,
    activate_object_GetItemType,
    activate_object_CompareItem,
    activate_object_Compare,
    activate_object_GetUINT32,
    activate_object_GetUINT64,
    activate_object_GetDouble,
    activate_object_GetGUID,
    activate_object_GetStringLength,
    activate_object_GetString,
    activate_object_GetAllocatedString,
    activate_object_GetBlobSize,
    activate_object_GetBlob,
    activate_object_GetAllocatedBlob,
    activate_object_GetUnknown,
    activate_object_SetItem,
    activate_object_DeleteItem,
    activate_object_DeleteAllItems,
    activate_object_SetUINT32,
    activate_object_SetUINT64,
    activate_object_SetDouble,
    activate_object_SetGUID,
    activate_object_SetString,
    activate_object_SetBlob,
    activate_object_SetUnknown,
    activate_object_LockStore,
    activate_object_UnlockStore,
    activate_object_GetCount,
    activate_object_GetItemByIndex,
    activate_object_CopyAllItems,
    activate_object_ActivateObject,
    activate_object_ShutdownObject,
    activate_object_DetachObject,
};

HRESULT create_activation_object(void *context, const struct activate_funcs *funcs, IMFActivate **ret)
{
    struct activate_object *object;
    HRESULT hr;

    object = heap_alloc_zero(sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IMFActivate_iface.lpVtbl = &activate_object_vtbl;
    object->refcount = 1;
    if (FAILED(hr = MFCreateAttributes(&object->attributes, 0)))
    {
        heap_free(object);
        return hr;
    }
    object->funcs = funcs;
    object->context = context;

    *ret = &object->IMFActivate_iface;

    return S_OK;
}

struct class_factory
{
    IClassFactory IClassFactory_iface;
    HRESULT (*create_instance)(REFIID riid, void **obj);
};

static inline struct class_factory *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct class_factory, IClassFactory_iface);
}

static HRESULT WINAPI class_factory_QueryInterface(IClassFactory *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualGUID(riid, &IID_IClassFactory) ||
            IsEqualGUID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IClassFactory_AddRef(iface);
        return S_OK;
    }

    WARN("%s is not supported.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI class_factory_AddRef(IClassFactory *iface)
{
    return 2;
}

static ULONG WINAPI class_factory_Release(IClassFactory *iface)
{
    return 1;
}

static HRESULT WINAPI class_factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **obj)
{
    struct class_factory *factory = impl_from_IClassFactory(iface);

    TRACE("%p, %p, %s, %p.\n", iface, outer, debugstr_guid(riid), obj);

    if (outer)
    {
        *obj = NULL;
        return CLASS_E_NOAGGREGATION;
    }

    return factory->create_instance(riid, obj);
}

static HRESULT WINAPI class_factory_LockServer(IClassFactory *iface, BOOL dolock)
{
    FIXME("%d.\n", dolock);

    return S_OK;
}

static const IClassFactoryVtbl class_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    class_factory_CreateInstance,
    class_factory_LockServer,
};

struct file_scheme_handler
{
    IMFSchemeHandler IMFSchemeHandler_iface;
    LONG refcount;
    struct handler handler;
    IMFSourceResolver *resolver;
};

static struct file_scheme_handler *impl_from_IMFSchemeHandler(IMFSchemeHandler *iface)
{
    return CONTAINING_RECORD(iface, struct file_scheme_handler, IMFSchemeHandler_iface);
}

static HRESULT WINAPI file_scheme_handler_QueryInterface(IMFSchemeHandler *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFSchemeHandler) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFSchemeHandler_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI file_scheme_handler_AddRef(IMFSchemeHandler *iface)
{
    struct file_scheme_handler *handler = impl_from_IMFSchemeHandler(iface);
    ULONG refcount = InterlockedIncrement(&handler->refcount);

    TRACE("%p, refcount %u.\n", handler, refcount);

    return refcount;
}

static ULONG WINAPI file_scheme_handler_Release(IMFSchemeHandler *iface)
{
    struct file_scheme_handler *this = impl_from_IMFSchemeHandler(iface);
    ULONG refcount = InterlockedDecrement(&this->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        if (this->resolver)
            IMFSourceResolver_Release(this->resolver);
        handler_destruct(&this->handler);
    }

    return refcount;
}

static HRESULT WINAPI file_scheme_handler_BeginCreateObject(IMFSchemeHandler *iface, const WCHAR *url, DWORD flags,
        IPropertyStore *props, IUnknown **cancel_cookie, IMFAsyncCallback *callback, IUnknown *state)
{
    struct file_scheme_handler *this = impl_from_IMFSchemeHandler(iface);

    TRACE("%p, %s, %#x, %p, %p, %p, %p.\n", iface, debugstr_w(url), flags, props, cancel_cookie, callback, state);
    return handler_begin_create_object(&this->handler, NULL, url, flags, props, cancel_cookie, callback, state);
}

static HRESULT WINAPI file_scheme_handler_EndCreateObject(IMFSchemeHandler *iface, IMFAsyncResult *result,
        MF_OBJECT_TYPE *obj_type, IUnknown **object)
{
    struct file_scheme_handler *this = impl_from_IMFSchemeHandler(iface);

    TRACE("%p, %p, %p, %p.\n", iface, result, obj_type, object);
    return handler_end_create_object(&this->handler, result, obj_type, object);
}

static HRESULT WINAPI file_scheme_handler_CancelObjectCreation(IMFSchemeHandler *iface, IUnknown *cancel_cookie)
{
    struct file_scheme_handler *this = impl_from_IMFSchemeHandler(iface);

    TRACE("%p, %p.\n", iface, cancel_cookie);
    return handler_cancel_object_creation(&this->handler, cancel_cookie);
}

static const IMFSchemeHandlerVtbl file_scheme_handler_vtbl =
{
    file_scheme_handler_QueryInterface,
    file_scheme_handler_AddRef,
    file_scheme_handler_Release,
    file_scheme_handler_BeginCreateObject,
    file_scheme_handler_EndCreateObject,
    file_scheme_handler_CancelObjectCreation,
};

static HRESULT file_scheme_handler_get_resolver(struct file_scheme_handler *handler, IMFSourceResolver **resolver)
{
    HRESULT hr;

    if (!handler->resolver)
    {
        IMFSourceResolver *resolver;

        if (FAILED(hr = MFCreateSourceResolver(&resolver)))
            return hr;

        if (InterlockedCompareExchangePointer((void **)&handler->resolver, resolver, NULL))
            IMFSourceResolver_Release(resolver);
    }

    *resolver = handler->resolver;
    IMFSourceResolver_AddRef(*resolver);

    return S_OK;
}

static HRESULT file_scheme_handler_create_object(struct handler *handler, WCHAR *url, IMFByteStream *stream, DWORD flags,
                                         IPropertyStore *props, IUnknown **out_object, MF_OBJECT_TYPE *out_obj_type)
{
    static const WCHAR schemeW[] = {'f','i','l','e',':','/','/'};
    HRESULT hr = S_OK;
    WCHAR *path;
    IMFByteStream *file_byte_stream;
    struct file_scheme_handler *this = CONTAINING_RECORD(handler, struct file_scheme_handler, handler);
    IMFSourceResolver *resolver;

    /* Strip from scheme, MFCreateFile() won't be expecting it. */
    path = url;
    if (!wcsnicmp(url, schemeW, ARRAY_SIZE(schemeW)))
        path += ARRAY_SIZE(schemeW);

    hr = MFCreateFile(flags & MF_RESOLUTION_WRITE ? MF_ACCESSMODE_READWRITE : MF_ACCESSMODE_READ,
            MF_OPENMODE_FAIL_IF_NOT_EXIST, MF_FILEFLAGS_NONE, path, &file_byte_stream);
    if (SUCCEEDED(hr))
    {
        if (flags & MF_RESOLUTION_MEDIASOURCE)
        {
            if (SUCCEEDED(hr = file_scheme_handler_get_resolver(this, &resolver)))
            {
                hr = IMFSourceResolver_CreateObjectFromByteStream(resolver, file_byte_stream, url, flags,
                        props, out_obj_type, out_object);
                IMFSourceResolver_Release(resolver);
                IMFByteStream_Release(file_byte_stream);
            }
        }
        else
        {
            *out_object = (IUnknown *)file_byte_stream;
            *out_obj_type = MF_OBJECT_BYTESTREAM;
        }
    }

    return hr;
}

static HRESULT file_scheme_handler_construct(REFIID riid, void **obj)
{
    struct file_scheme_handler *this;
    HRESULT hr;

    TRACE("%s, %p.\n", debugstr_guid(riid), obj);

    this = heap_alloc_zero(sizeof(*this));
    if (!this)
        return E_OUTOFMEMORY;

    handler_construct(&this->handler, file_scheme_handler_create_object);

    this->IMFSchemeHandler_iface.lpVtbl = &file_scheme_handler_vtbl;
    this->refcount = 1;

    hr = IMFSchemeHandler_QueryInterface(&this->IMFSchemeHandler_iface, riid, obj);
    IMFSchemeHandler_Release(&this->IMFSchemeHandler_iface);

    return hr;
}

static struct class_factory file_scheme_handler_factory = { { &class_factory_vtbl }, file_scheme_handler_construct };

static const struct class_object
{
    const GUID *clsid;
    IClassFactory *factory;
}
class_objects[] =
{
    { &CLSID_FileSchemeHandler, &file_scheme_handler_factory.IClassFactory_iface },
};

/*******************************************************************************
 *      DllGetClassObject (mf.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **obj)
{
    unsigned int i;

    TRACE("%s, %s, %p.\n", debugstr_guid(rclsid), debugstr_guid(riid), obj);

    for (i = 0; i < ARRAY_SIZE(class_objects); ++i)
    {
        if (IsEqualGUID(class_objects[i].clsid, rclsid))
            return IClassFactory_QueryInterface(class_objects[i].factory, riid, obj);
    }

    WARN("%s: class not found.\n", debugstr_guid(rclsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/******************************************************************
 *              DllCanUnloadNow (mf.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

/***********************************************************************
 *          DllRegisterServer (mf.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources( mf_instance );
}

/***********************************************************************
 *          DllUnregisterServer (mf.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( mf_instance );
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;    /* prefer native version */
        case DLL_PROCESS_ATTACH:
            mf_instance = instance;
            DisableThreadLibraryCalls(instance);
            break;
    }

    return TRUE;
}

/***********************************************************************
 *      MFGetSupportedMimeTypes (mf.@)
 */
HRESULT WINAPI MFGetSupportedMimeTypes(PROPVARIANT *array)
{
    FIXME("(%p) stub\n", array);

    return E_NOTIMPL;
}

/***********************************************************************
 *      MFGetService (mf.@)
 */
HRESULT WINAPI MFGetService(IUnknown *object, REFGUID service, REFIID riid, void **obj)
{
    IMFGetService *gs;
    HRESULT hr;

    TRACE("(%p, %s, %s, %p)\n", object, debugstr_guid(service), debugstr_guid(riid), obj);

    if (!object)
        return E_POINTER;

    if (FAILED(hr = IUnknown_QueryInterface(object, &IID_IMFGetService, (void **)&gs)))
        return hr;

    hr = IMFGetService_GetService(gs, service, riid, obj);
    IMFGetService_Release(gs);
    return hr;
}

/***********************************************************************
 *      MFShutdownObject (mf.@)
 */
HRESULT WINAPI MFShutdownObject(IUnknown *object)
{
    IMFShutdown *shutdown;

    TRACE("%p.\n", object);

    if (object && SUCCEEDED(IUnknown_QueryInterface(object, &IID_IMFShutdown, (void **)&shutdown)))
    {
        IMFShutdown_Shutdown(shutdown);
        IMFShutdown_Release(shutdown);
    }

    return S_OK;
}

/***********************************************************************
 *      MFEnumDeviceSources (mf.@)
 */
HRESULT WINAPI MFEnumDeviceSources(IMFAttributes *attributes, IMFActivate ***sources, UINT32 *count)
{
    FIXME("%p, %p, %p.\n", attributes, sources, count);

    if (!attributes || !sources || !count)
        return E_INVALIDARG;

    *count = 0;

    return S_OK;
}

static HRESULT evr_create_object(IMFAttributes *attributes, void *user_context, IUnknown **obj)
{
    FIXME("%p, %p, %p.\n", attributes, user_context, obj);

    return E_NOTIMPL;
}

static void evr_free_private(void *user_context)
{
}

static const struct activate_funcs evr_activate_funcs =
{
    evr_create_object,
    evr_free_private,
};

HRESULT WINAPI MFCreateVideoRendererActivate(HWND hwnd, IMFActivate **activate)
{
    TRACE("%p, %p.\n", hwnd, activate);

    return create_activation_object(hwnd, &evr_activate_funcs, activate);
}
