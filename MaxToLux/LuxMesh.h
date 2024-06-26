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

#pragma once
#include <algorithm>
#include <max.h>
#include <luxcore.h>
// #include <luxrays\luxrays.h>
#include "Classes.h"

class MaxToLuxMesh
{
public:
	CRITICAL_SECTION meshcsect;
	void createMeshesInGroup(INode *currNode, luxcore::Scene &scene, TimeValue t);
	void createMesh(INode *node, luxcore::Scene &scene, TimeValue t, bool prev = false);
	// The key contains the node pool from which the mesh is queried

private:

};

