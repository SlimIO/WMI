#define _WIN32_DCOM
#include <iostream>
#include <sstream>
#include <comdef.h>
#include <Wbemidl.h>
#include "napi.h"

#pragma comment(lib, "wbemuuid.lib")

/**
 * @doc: https://docs.microsoft.com/en-us/windows/win32/wmisdk/example--getting-wmi-data-from-the-local-computer
 * @doc: https://www.activexperts.com/admin/scripts/wmi/python/ (list of WMI tables)
 */
Napi::Value execQuery(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    std::stringstream error;
    HRESULT hres;

    // Step 1: --------------------------------------------------
    // Initialize COM. ------------------------------------------
    hres =  CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        error << "Failed to initialize COM library. Error code = 0x" << std::hex << hres << std::endl;
        Napi::Error::New(env, error.str()).ThrowAsJavaScriptException();

        return env.Null();
    }

    // Step 2: --------------------------------------------------
    // Set general COM security levels --------------------------
    hres =  CoInitializeSecurity(
        NULL,
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities
        NULL                         // Reserved
    );
    if (FAILED(hres)) {
        error << "Failed to initialize security. Error code = 0x" << std::hex << hres << std::endl;
        Napi::Error::New(env, error.str()).ThrowAsJavaScriptException();
        CoUninitialize();

        return env.Null();
    }

    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI -------------------------
    IWbemLocator *pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pLoc);
    if (FAILED(hres)) {
        error << "Failed to create IWbemLocator object." << " Err code = 0x" << std::hex << hres << std::endl;
        Napi::Error::New(env, error.str()).ThrowAsJavaScriptException();
        CoUninitialize();

        return env.Null();
    }

    // Step 4: -----------------------------------------------------
    // Connect to WMI through the IWbemLocator::ConnectServer method
    // Connect to the root\cimv2 namespace with the current user and obtain pointer pSvc to make IWbemServices calls.
    IWbemServices *pSvc = NULL;
    hres = pLoc->ConnectServer(
         _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
         NULL,                    // User name. NULL = current user
         NULL,                    // User password. NULL = current
         0,                       // Locale. NULL indicates current
         NULL,                    // Security flags.
         0,                       // Authority (for example, Kerberos)
         0,                       // Context object
         &pSvc                    // pointer to IWbemServices proxy
    );
    if (FAILED(hres)) {
        error << "Could not connect. Error code = 0x" << std::hex << hres << std::endl;
        Napi::Error::New(env, error.str()).ThrowAsJavaScriptException();
        pLoc->Release();
        CoUninitialize();

        return env.Null();
    }

    std::cout << "Connected to ROOT\\CIMV2 WMI namespace" << std::endl;

    // Step 5: --------------------------------------------------
    // Set security levels on the proxy -------------------------
    hres = CoSetProxyBlanket(
       pSvc,                        // Indicates the proxy to set
       RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
       RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
       NULL,                        // Server principal name
       RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
       RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
       NULL,                        // client identity
       EOAC_NONE                    // proxy capabilities
    );
    if (FAILED(hres)) {
        error << "Could not set proxy blanket. Error code = 0x" << std::hex << hres << std::endl;
        Napi::Error::New(env, error.str()).ThrowAsJavaScriptException();
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();

        return env.Null();
    }

    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----
    // For example, get the name of the operating system
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_OperatingSystem"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres)) {
        error << "Query for operating system name failed." << " Error code = 0x" << std::hex << hres << std::endl;
        Napi::Error::New(env, error.str()).ThrowAsJavaScriptException();
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();

        return env.Null();
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------
    IWbemClassObject *pclsObj = NULL;
    Napi::Object ret = Napi::Object::New(env);
    ULONG uReturn = 0;

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if(uReturn == 0) {
            break;
        }

        VARIANT vtProp;

        // Get the value of the Name property
        hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
        std::wcout << " OS Name : " << vtProp.bstrVal << std::endl;
        VariantClear(&vtProp);

        pclsObj->Release();
    }

    // Cleanup
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return ret;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("execQuery", Napi::Function::New(env, execQuery));

    return exports;
}

NODE_API_MODULE(wmi, Init);
