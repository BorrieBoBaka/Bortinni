/**
 * MIT License
 *
 * Copyright (c) 2020 Philip Klatt
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
**/

#include "utinni.h"
#include "depth_texture.h"

#include <d3dx9.h>
#include <d3d9types.h>
#include "external/nvapi/nvapi.h"

#pragma comment(lib, "nvapi/x86/nvapi.lib")

#define FOURCC_INTZ ((D3DFORMAT)(MAKEFOURCC('I','N','T','Z')))
#define RESZ_CODE 0x7fa05000

namespace directX
{
DepthTexture::DepthTexture()
    : pTexture(nullptr)
	 , m_isNVAPI(false)
	 , m_isSupported(false)
	 , pRegisteredDSS(nullptr)
{
	 if (NvAPI_Initialize() == NVAPI_OK)
	 {
		  m_isNVAPI = true;
	 }
	 m_isRESZ = !m_isNVAPI;

	 m_isSupported = (m_isNVAPI || m_isRESZ); // determine if RESZ(ATI) or NVAPI supported
}

//--------------------------------------------------------------------------------------
void DepthTexture::createTexture(LPDIRECT3DDEVICE9 device, int width, int height)
{
	 if (m_isSupported)
	 {
		  D3DFORMAT format = FOURCC_INTZ; //m_isNVAPI ? FOURCC_INTZ : FOURCC_RAWZ;
		  device->CreateTexture(width, height, 1, D3DUSAGE_DEPTHSTENCIL, format, D3DPOOL_DEFAULT, &pTexture, NULL);

		  if (m_isNVAPI)
		  {
				//utinni::log::info("nVidia GPU Selected");
				NvAPI_D3D9_RegisterResource(pTexture);
		  }
		  /*else
		  {
				utinni::log::info("ATI or Intel GPU Selected");
		  }*/
	 }
}

void DepthTexture::release()
{
	 if (pTexture)
	 {
		  if (m_isNVAPI)
		  {
				NvAPI_D3D9_UnregisterResource(pTexture);
		  }
		  if (pTexture != nullptr)
		  {
				pTexture->Release();
				pTexture = nullptr;
		  }
	 }

	 if (pRegisteredDSS)
	 {
		  if (m_isNVAPI)
		  {
				NvAPI_D3D9_UnregisterResource(pRegisteredDSS);
		  }
		  if (pRegisteredDSS != nullptr)
		  {
				pRegisteredDSS->Release();
				pRegisteredDSS = nullptr;
		  }
	 }
}


DepthTexture::~DepthTexture()
{
	 release();
}

void resolveDepthWithResz(const LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9 pTexture)
{
	 pDevice->SetVertexShader(nullptr);
	 pDevice->SetPixelShader(nullptr);
	 pDevice->SetFVF(D3DFVF_XYZ);

	 // Bind depth stencil texture to texture sampler 0
	 pDevice->SetTexture(0, pTexture);

	 // Perform a dummy draw call to ensure texture sampler 0 is set before the // resolve is triggered
	 // Vertex declaration and shaders may need to be adjusted to ensure no debug
	 // error message is produced
	 D3DXVECTOR3 vDummyPoint(0.0f, 0.0f, 0.0f);
	 pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	 pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	 pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
	 pDevice->DrawPrimitiveUP(D3DPT_POINTLIST, 1, vDummyPoint, sizeof(D3DXVECTOR3));
	 pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	 pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	 pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);

	 // Trigger the depth buffer resolve; after this call texture sampler 0
	 // will contain the contents of the resolve operation
	 pDevice->SetRenderState(D3DRS_POINTSIZE, RESZ_CODE);

	 // This hack to fix resz hack, has been found by Maksym Bezus!!!
	 // Without this line resz will be resolved only for first frame
	 pDevice->SetRenderState(D3DRS_POINTSIZE, 0);
}

void DepthTexture::resolveDepth(const LPDIRECT3DDEVICE9 pDevice, IDirect3DSurface9* pSurface)
{
	 if (m_isNVAPI)
	 {
		  //Safety catch just incase
		  if (pRegisteredDSS != pSurface)
		  {
				//Registering Fails, something is up with the depth stencil surface
				NvAPI_D3D9_RegisterResource(pSurface);
				if (pRegisteredDSS != nullptr)
				{
					 // Unregister old one if there is any
					 NvAPI_D3D9_UnregisterResource(pRegisteredDSS);
				}
				pRegisteredDSS = pSurface;
		  }
		  DWORD result = NvAPI_D3D9_StretchRectEx(pDevice, pSurface, nullptr, pTexture, nullptr, D3DTEXF_NONE);
	 }
    else if (m_isRESZ)
	 {
		  resolveDepthWithResz(pDevice, pTexture);
	 }
}

void DepthTexture::resolveDepth(const LPDIRECT3DDEVICE9 pDevice)
{
	 if (m_isNVAPI)
	 {
		  IDirect3DSurface9* pDSS = nullptr;
		  pDevice->GetDepthStencilSurface(&pDSS);
		  if (pRegisteredDSS != pDSS)
		  {
				//Registering Fails, something is up with the depth stencil surface
				NvAPI_D3D9_RegisterResource(pDSS);
				if (pRegisteredDSS != nullptr)
				{
					 // Unregister old one if there is any
					 NvAPI_D3D9_UnregisterResource(pRegisteredDSS);
				}
				pRegisteredDSS = pDSS;
		  }
		  DWORD result = NvAPI_D3D9_StretchRectEx(pDevice, pDSS, nullptr, pTexture, nullptr, D3DTEXF_NONE);
		  pDSS->Release();
	 }
	 else if (m_isRESZ)
	 {
		  resolveDepthWithResz(pDevice, pTexture);
	 }
}
}