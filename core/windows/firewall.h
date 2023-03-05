#include <crtdbg.h>
#include <netfw.h>
#include <objbase.h>
#include <oleauto.h>
#include <stdio.h>

#pragma comment( lib, "ole32.lib" )
#pragma comment( lib, "oleaut32.lib" )

HRESULT WindowsFirewallInitialize(OUT INetFwProfile** fwProfile)
{
    HRESULT hr = S_OK;
    INetFwMgr* fwMgr = NULL;
    INetFwPolicy* fwPolicy = NULL;

    _ASSERT(fwProfile != NULL);

    *fwProfile = NULL;

    hr = CoCreateInstance(
            __uuidof(NetFwMgr),
            NULL,
            CLSCTX_INPROC_SERVER,
            __uuidof(INetFwMgr),
            (void**)&fwMgr
            );
    if (FAILED(hr))
    {
        printf("CoCreateInstance failed: 0x%08lx\n", hr);
        goto error;
    }

    hr = fwMgr->get_LocalPolicy(&fwPolicy);
    if (FAILED(hr))
    {
        printf("get_LocalPolicy failed: 0x%08lx\n", hr);
        goto error;
    }

    hr = fwPolicy->get_CurrentProfile(fwProfile);
    if (FAILED(hr))
    {
        printf("get_CurrentProfile failed: 0x%08lx\n", hr);
        goto error;
    }

error:

    if (fwPolicy != NULL)
    {
        fwPolicy->Release();
    }

    if (fwMgr != NULL)
    {
        fwMgr->Release();
    }

    return hr;
}


void WindowsFirewallCleanup(IN INetFwProfile* fwProfile)
{
    if (fwProfile != NULL)
    {
        fwProfile->Release();
    }
}


HRESULT WindowsFirewallIsOn(IN INetFwProfile* fwProfile, OUT BOOL* fwOn)
{
    HRESULT hr = S_OK;
    VARIANT_BOOL fwEnabled;

    _ASSERT(fwProfile != NULL);
    _ASSERT(fwOn != NULL);

    *fwOn = FALSE;

    hr = fwProfile->get_FirewallEnabled(&fwEnabled);
    if (FAILED(hr))
    {
        printf("get_FirewallEnabled failed: 0x%08lx\n", hr);
        goto error;
    }

    if (fwEnabled != VARIANT_FALSE)
    {
        *fwOn = TRUE;
        printf("The firewall is on.\n");
    }
    else
    {
        printf("The firewall is off.\n");
    }

error:

    return hr;
}

HRESULT WindowsFirewallAppIsEnabled(
            IN INetFwProfile* fwProfile,
            IN const wchar_t* fwProcessImageFileName,
            OUT BOOL* fwAppEnabled
            )
{
    HRESULT hr = S_OK;
    BSTR fwBstrProcessImageFileName = NULL;
    VARIANT_BOOL fwEnabled;
    INetFwAuthorizedApplication* fwApp = NULL;
    INetFwAuthorizedApplications* fwApps = NULL;

    _ASSERT(fwProfile != NULL);
    _ASSERT(fwProcessImageFileName != NULL);
    _ASSERT(fwAppEnabled != NULL);

    *fwAppEnabled = FALSE;

    hr = fwProfile->get_AuthorizedApplications(&fwApps);
    if (FAILED(hr))
    {
        printf("get_AuthorizedApplications failed: 0x%08lx\n", hr);
        goto error;
    }

    fwBstrProcessImageFileName = SysAllocString(fwProcessImageFileName);
    if (fwBstrProcessImageFileName == NULL)
    {
        hr = E_OUTOFMEMORY;
        printf("SysAllocString failed: 0x%08lx\n", hr);
        goto error;
    }

    hr = fwApps->Item(fwBstrProcessImageFileName, &fwApp);
    if (SUCCEEDED(hr))
    {
        hr = fwApp->get_Enabled(&fwEnabled);
        if (FAILED(hr))
        {
            printf("get_Enabled failed: 0x%08lx\n", hr);
            goto error;
        }

        if (fwEnabled != VARIANT_FALSE)
        {
            *fwAppEnabled = TRUE;

            printf(
                "Authorized application %lS is enabled in the firewall.\n",
                fwProcessImageFileName
                );
        }
        else
        {
            printf(
                "Authorized application %lS is disabled in the firewall.\n",
                fwProcessImageFileName
                );
        }
    }
    else
    {
        hr = S_OK;

        printf(
            "Authorized application %lS is disabled in the firewall.\n",
            fwProcessImageFileName
            );
    }

error:

    SysFreeString(fwBstrProcessImageFileName);

    if (fwApp != NULL)
    {
        fwApp->Release();
    }

    if (fwApps != NULL)
    {
        fwApps->Release();
    }

    return hr;
}


HRESULT WindowsFirewallAddApp(
            IN INetFwProfile* fwProfile,
            IN const wchar_t* fwProcessImageFileName,
            IN const wchar_t* fwName
            )
{
    HRESULT hr = S_OK;
    BOOL fwAppEnabled;
    BSTR fwBstrName = NULL;
    BSTR fwBstrProcessImageFileName = NULL;
    INetFwAuthorizedApplication* fwApp = NULL;
    INetFwAuthorizedApplications* fwApps = NULL;

    _ASSERT(fwProfile != NULL);
    _ASSERT(fwProcessImageFileName != NULL);
    _ASSERT(fwName != NULL);

    hr = WindowsFirewallAppIsEnabled(
            fwProfile,
            fwProcessImageFileName,
            &fwAppEnabled
            );
    if (FAILED(hr))
    {
        printf("WindowsFirewallAppIsEnabled failed: 0x%08lx\n", hr);
        goto error;
    }

    if (!fwAppEnabled)
    {
        hr = fwProfile->get_AuthorizedApplications(&fwApps);
        if (FAILED(hr))
        {
            printf("get_AuthorizedApplications failed: 0x%08lx\n", hr);
            goto error;
        }

        hr = CoCreateInstance(
                __uuidof(NetFwAuthorizedApplication),
                NULL,
                CLSCTX_INPROC_SERVER,
                __uuidof(INetFwAuthorizedApplication),
                (void**)&fwApp
                );
        if (FAILED(hr))
        {
            printf("CoCreateInstance failed: 0x%08lx\n", hr);
            goto error;
        }

        fwBstrProcessImageFileName = SysAllocString(fwProcessImageFileName);
        if (fwBstrProcessImageFileName == NULL)
        {
            hr = E_OUTOFMEMORY;
            printf("SysAllocString failed: 0x%08lx\n", hr);
            goto error;
        }

        hr = fwApp->put_ProcessImageFileName(fwBstrProcessImageFileName);
        if (FAILED(hr))
        {
            printf("put_ProcessImageFileName failed: 0x%08lx\n", hr);
            goto error;
        }

        fwBstrName = SysAllocString(fwName);
        if (SysStringLen(fwBstrName) == 0)
        {
            hr = E_OUTOFMEMORY;
            printf("SysAllocString failed: 0x%08lx\n", hr);
            goto error;
        }

        hr = fwApp->put_Name(fwBstrName);
        if (FAILED(hr))
        {
            printf("put_Name failed: 0x%08lx\n", hr);
            goto error;
        }

        hr = fwApps->Add(fwApp);
        if (FAILED(hr))
        {
            printf("Add failed: 0x%08lx\n", hr);
            goto error;
        }

        printf(
            "Authorized application %lS is now enabled in the firewall.\n",
            fwProcessImageFileName
            );
    }

error:

    SysFreeString(fwBstrName);
    SysFreeString(fwBstrProcessImageFileName);

    if (fwApp != NULL)
    {
        fwApp->Release();
    }

    if (fwApps != NULL)
    {
        fwApps->Release();
    }

    return hr;
}


HRESULT WindowsFirewallPortIsEnabled(
            IN INetFwProfile* fwProfile,
            IN LONG portNumber,
            IN NET_FW_IP_PROTOCOL ipProtocol,
            OUT BOOL* fwPortEnabled
            )
{
    HRESULT hr = S_OK;
    VARIANT_BOOL fwEnabled;
    INetFwOpenPort* fwOpenPort = NULL;
    INetFwOpenPorts* fwOpenPorts = NULL;

    _ASSERT(fwProfile != NULL);
    _ASSERT(fwPortEnabled != NULL);

    *fwPortEnabled = FALSE;

    hr = fwProfile->get_GloballyOpenPorts(&fwOpenPorts);
    if (FAILED(hr))
    {
        printf("get_GloballyOpenPorts failed: 0x%08lx\n", hr);
        goto error;
    }

    hr = fwOpenPorts->Item(portNumber, ipProtocol, &fwOpenPort);
    if (SUCCEEDED(hr))
    {
        hr = fwOpenPort->get_Enabled(&fwEnabled);
        if (FAILED(hr))
        {
            printf("get_Enabled failed: 0x%08lx\n", hr);
            goto error;
        }

        if (fwEnabled != VARIANT_FALSE)
        {
            *fwPortEnabled = TRUE;

            printf("Port %ld is open in the firewall.\n", portNumber);
        }
        else
        {
            printf("Port %ld is not open in the firewall.\n", portNumber);
        }
    }
    else
    {
        hr = S_OK;

        printf("Port %ld is not open in the firewall.\n", portNumber);
    }

error:

    if (fwOpenPort != NULL)
    {
        fwOpenPort->Release();
    }

    if (fwOpenPorts != NULL)
    {
        fwOpenPorts->Release();
    }

    return hr;
}


HRESULT WindowsFirewallPortAdd(
            IN INetFwProfile* fwProfile,
            IN LONG portNumber,
            IN NET_FW_IP_PROTOCOL ipProtocol,
            IN const wchar_t* name
            )
{
    HRESULT hr = S_OK;
    BOOL fwPortEnabled;
    BSTR fwBstrName = NULL;
    INetFwOpenPort* fwOpenPort = NULL;
    INetFwOpenPorts* fwOpenPorts = NULL;

    _ASSERT(fwProfile != NULL);
    _ASSERT(name != NULL);

    hr = WindowsFirewallPortIsEnabled(
            fwProfile,
            portNumber,
            ipProtocol,
            &fwPortEnabled
            );
    if (FAILED(hr))
    {
        printf("WindowsFirewallPortIsEnabled failed: 0x%08lx\n", hr);
        goto error;
    }

    if (!fwPortEnabled)
    {
        hr = fwProfile->get_GloballyOpenPorts(&fwOpenPorts);
        if (FAILED(hr))
        {
            printf("get_GloballyOpenPorts failed: 0x%08lx\n", hr);
            goto error;
        }

        hr = CoCreateInstance(
                __uuidof(NetFwOpenPort),
                NULL,
                CLSCTX_INPROC_SERVER,
                __uuidof(INetFwOpenPort),
                (void**)&fwOpenPort
                );
        if (FAILED(hr))
        {
            printf("CoCreateInstance failed: 0x%08lx\n", hr);
            goto error;
        }

        hr = fwOpenPort->put_Port(portNumber);
        if (FAILED(hr))
        {
            printf("put_Port failed: 0x%08lx\n", hr);
            goto error;
        }

        hr = fwOpenPort->put_Protocol(ipProtocol);
        if (FAILED(hr))
        {
            printf("put_Protocol failed: 0x%08lx\n", hr);
            goto error;
        }

        fwBstrName = SysAllocString(name);
        if (SysStringLen(fwBstrName) == 0)
        {
            hr = E_OUTOFMEMORY;
            printf("SysAllocString failed: 0x%08lx\n", hr);
            goto error;
        }

        hr = fwOpenPort->put_Name(fwBstrName);
        if (FAILED(hr))
        {
            printf("put_Name failed: 0x%08lx\n", hr);
            goto error;
        }

        hr = fwOpenPorts->Add(fwOpenPort);
        if (FAILED(hr))
        {
            printf("Add failed: 0x%08lx\n", hr);
            goto error;
        }

        printf("Port %ld is now open in the firewall.\n", portNumber);
    }

error:

    SysFreeString(fwBstrName);

    if (fwOpenPort != NULL)
    {
        fwOpenPort->Release();
    }

    if (fwOpenPorts != NULL)
    {
        fwOpenPorts->Release();
    }

    return hr;
}
