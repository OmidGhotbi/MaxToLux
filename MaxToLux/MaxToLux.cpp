/**************************************************************************
* Copyright (c) 2015-2024 Luxrender.                                      *
* All rights reserved.                                                    *
*                                                                         *
* DESCRIPTION: Contains the Dll Entry stuff                               *
* AUTHOR: Omid Ghotbi (TAO) omid.ghotbi@gmail.com                         *
*                                                                         *
*   This file is part of LuxRender.                                       *
*                                                                         *
* Licensed under the Apache License, Version 2.0 (the "License");         *
* you may not use this file except in compliance with the License.        *
* You may obtain a copy of the License at                                 *
*                                                                         *
*     http://www.apache.org/licenses/LICENSE-2.0                          *
*                                                                         *
* Unless required by applicable law or agreed to in writing, software     *
* distributed under the License is distributed on an "AS IS" BASIS,       *
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
* See the License for the specific language governing permissions and     *
* limitations under the License.                                          *
***************************************************************************/


#define CAMERA_TARGER_CLASSID	Class_ID(4128,0)
#define MAX2016_PHYSICAL_CAMERA Class_ID(1181315608, 686293133)
#define ENV_FOG_CLASS_ID		Class_ID(268435457, 0) 

#include <notify.h>
#include <limits>
#include "main.h"
#include "MaxToLux.h"
#include "maxscript/maxscript.h"

#include <vector>
#include <thread>

MaxToLuxCamera lxmCamera;
MaxToLuxLights lxmLights;
MaxToLuxMaterials lxmMaterials;
MaxToLuxUtils lxmUtils;
MaxToLuxMesh lxmMesh;

namespace luxcore
{
	#include <luxcore.h>
}

using namespace luxcore;
using namespace luxrays;

extern BOOL FileExists(const TCHAR *filename);
float* pixels;
bool defaultlightset = true;

int filterIndex;
//bool enableFileSaverOutput;

class MaxToLuxClassDesc :public ClassDesc2 {
public:
	virtual int 			IsPublic() { return 1; }
	virtual void *			Create(BOOL loading) { return new MaxToLux(loading); }
	//virtual void *			Create(BOOL loading) override final;
	virtual const TCHAR *	ClassName() { return GetString(IDS_VRENDTITLE); }
#if GET_MAX_RELEASE(VERSION_3DSMAX) >= 23900
	const TCHAR*  NonLocalizedClassName() override { return GetString(IDS_VRENDTITLE); }
#endif
	virtual SClass_ID		SuperClassID() { return RENDERER_CLASS_ID; }
	virtual Class_ID 		ClassID() { return REND_CLASS_ID; }
	virtual const TCHAR* 	Category() { return _T("LuxCoreRenderer"); }
	virtual void			ResetClassParams(BOOL fileReset) { UNREFERENCED_PARAMETER(fileReset); }
};

ClassDesc2* GetRendDesc() {
	static MaxToLuxClassDesc srendCD;
	return &srendCD;
}

MaxToLux::MaxToLux(BOOL loading)
{
	
}

MaxToLux::MaxToLux(IRenderSessionContext& sessionContext, const bool is_interactive_session)
	: m_rendering_process(sessionContext.GetRenderingProcess())
{
	file = NULL;
	sceneNode = NULL;
	viewNode = NULL;
	anyLights = FALSE;
	nlts = nobs = 0;
}

RefResult MaxToLux::NotifyRefChanged(const Interval &changeInt, RefTargetHandle hTarget, PartID &partID,
	RefMessage message, BOOL propagate)
{
	UNREFERENCED_PARAMETER(propagate);
	UNREFERENCED_PARAMETER(message);
	UNREFERENCED_PARAMETER(partID);
	UNREFERENCED_PARAMETER(hTarget);
	UNREFERENCED_PARAMETER(changeInt);
	switch (message)
	{
		case REFMSG_CHANGE:
		{
			if (hTarget == pblock)
			{
				ParamID changing_param = pblock->LastNotifyParamID();
				DepthOfFieldblk.InvalidateUI(changing_param);
			}
			break;
		}
	}
	return REF_SUCCEED;
}

//::Matrix3 camPos;

int MaxToLux::Open(INode *scene, INode *vnode, ViewParams *viewPar, RendParams &rp, HWND hwnd, DefaultLight* defaultLights, int numDefLights, RendProgressCallback* prog)
{
	UNREFERENCED_PARAMETER(prog);
	UNREFERENCED_PARAMETER(numDefLights);
	UNREFERENCED_PARAMETER(defaultLights);
	UNREFERENCED_PARAMETER(hwnd);
	
	if (rp.inMtlEdit)
	{
		
		renderingMaterialPreview = true;
		materialPreviewScene = Scene::Create();

		lxmMesh.createMesh(scene, *materialPreviewScene, GetCOREInterface()->GetTime(), renderingMaterialPreview);
	}
	else
	{
		renderingMaterialPreview = false;
	}

	return 1;
}

void IMaxToLux::BeginSession()
{
}

void IMaxToLux::EndSession()
{
}