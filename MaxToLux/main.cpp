/**************************************************************************
* Copyright (c) 2015-2022 Luxrender.                                      *
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


#include "main.h"
#include <string>

extern ClassDesc2* GetRendDesc();
extern ClassDesc2* GetLR_NullDesc();

HINSTANCE hInstance;
HINSTANCE hInstanceAdvance;
int controlsInit = FALSE;

#define MAX_PLUGIN   2
ClassDesc *classDescArray[MAX_PLUGIN];
int classDescCount = 0;

static BOOL InitMtlDLL(void)
{
	if (!classDescCount)
	{
		classDescArray[classDescCount++] = GetRendDesc();
		classDescArray[classDescCount++] = GetLR_NullDesc();
	}

	return TRUE;
}