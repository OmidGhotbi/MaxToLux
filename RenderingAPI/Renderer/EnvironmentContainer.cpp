//////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2015 Autodesk, Inc.  All rights reserved.
//
//  Use of this software is subject to the terms of the Autodesk license 
//  agreement provided at the time of installation or download, or which 
//  otherwise accompanies this software in either electronic or hard copy form.   
//
//////////////////////////////////////////////////////////////////////////////
#include "EnvironmentContainer.h"

// local
#include "../resource.h"

// max sdk
#include <dllutilities.h>
#include <control.h>
#include <stdmat.h>
#include <maxapi.h>

namespace Max
{;
namespace RenderingAPI
{;

namespace
{
    // Guard which suspends animations (when setting up param block values) during its lifetime
    class AnimationSuspendGuard
    {
    public:
        AnimationSuspendGuard()
        {
            SuspendAnimate();
        }
        ~AnimationSuspendGuard()
        {
            ResumeAnimate();
        }
    };

    struct PBAndParam
    {
        IParamBlock2* param_block;
        ParamID param_id;
    };

    // Finds and returns the param block with the given internal name
    IParamBlock2* find_paramblock_by_name(ReferenceTarget& ref_targ, const MCHAR* name)
    {
        const int num_blocks = ref_targ.NumParamBlocks();
        for(int i = 0; i < num_blocks; ++i)
        {
            IParamBlock2* pb = ref_targ.GetParamBlock(i);
            if(pb != nullptr)
            {
                const MCHAR* pb_name = pb->GetDesc()->int_name;
                if((pb_name != nullptr) && (name != nullptr) && (_tcscmp(pb_name, name) == 0))
                {
                    return pb;
                }
            }
        }

        return nullptr;
    }

    // Returns whether the given texture is a mental ray env/background switcher
    bool is_mr_switcher_env(Texmap& texmap)
    {
        ClassDesc* const env_class_desc = GetCOREInterface()->GetDllDir().ClassDir().FindClass(texmap.SuperClassID(), texmap.ClassID());
        const MSTR internal_name = (env_class_desc!= nullptr) ? env_class_desc->InternalName() : nullptr;
        return (internal_name == _T("mip_rayswitch_environment"));
    }

    // Returns whether the given texture is set to use screen-mapped UV
    bool uses_screen_mapping(Texmap& tex)
    {
        UVGen* uv_gen = tex.GetTheUVGen();
        if((uv_gen != nullptr) && (uv_gen->IsStdUVGen()))
        {
            StdUVGen* const std_uv_gen = static_cast<StdUVGen*>(uv_gen);
            const int mapping = std_uv_gen->GetCoordMapping(0); 
            return (mapping == UVMAP_SCREEN_ENV);
        }

        return false;
    }
};

//==================================================================================================
// class EnvironmentContainer
//==================================================================================================

enum EnvironmentContainer::ParameterID
{
    ParamID_BackgroundMode = 0,
    ParamID_BackgroundColor = 1,
    ParamID_BackgroundTexture = 2,

    ParamID_EnvironmentMode = 3,
    ParamID_EnvironmentColor = 4,
    ParamID_EnvironmentTexture = 5,
};

// Declare the PBDesc here, rather than at global scope, to avoid constructing this thing on dll load.
ParamBlockDesc2 EnvironmentContainer::m_param_block_desc( 
        0,  // param block ID
        _T("EnvironmentContainerParams"),	// internal name
        IDS_ENVIRONMENT_CONTAINER_PB_NAME,  // resource ID for localizable name
        &get_class_descriptor(),           // class descriptor
        P_AUTO_CONSTRUCT,       // flags
        0,      // reference number (required because of P_AUTO_CONSTRUCT)

    ParamID_BackgroundMode, _T("background_mode"), TYPE_INT, 0, IDS_ENVIRONMENT_CONTAINER_PARAM_BACKGROUNDMODE,
        p_default, static_cast<int>(BackgroundMode::Environment),
        p_end,	
    
    ParamID_BackgroundColor, _T("background_color"), TYPE_FRGBA, P_ANIMATABLE, IDS_ENVIRONMENT_CONTAINER_PARAM_BACKGROUNDCOLOR,
        p_default, AColor(0.0f, 0.0f, 0.0f, 0.0f), 
        p_end,		

    ParamID_BackgroundTexture, _T("background_texture"), TYPE_TEXMAP, 0, IDS_ENVIRONMENT_CONTAINER_PARAM_BACKGROUNDTEXTURE,
        p_end,

    ParamID_EnvironmentMode, _T("environment_mode"), TYPE_INT, 0, IDS_ENVIRONMENT_CONTAINER_PARAM_ENVMODE,
        p_default, static_cast<int>(EnvironmentMode::None),
        p_end,

    ParamID_EnvironmentColor, _T("environment_color"), TYPE_FRGBA, P_ANIMATABLE, IDS_ENVIRONMENT_CONTAINER_PARAM_ENVCOLOR,
        p_default, AColor(0.0f, 0.0f, 0.0f, 0.0f),
        p_end,

    ParamID_EnvironmentTexture, _T("environment_texture"), TYPE_TEXMAP, 0, IDS_ENVIRONMENT_CONTAINER_PARAM_ENVTEXTURE,
        p_end,

    p_end							
);

EnvironmentContainer::EnvironmentContainer(const bool loading)
    : m_param_block(nullptr),
    m_legacy_environment(nullptr),
    m_use_legacy_environment(false)
{
    // Register the class with the system on first instantiation. This is necessary for compatibility with the AMG translator, which relies
    // on the class directory.
    // The class isn't automatically registered upon loading the DLL because the Rendering API is not a plugin dll.
    static bool class_registered = false;
    if(!class_registered)
    {
        class_registered = true;
        DbgVerify(GetCOREInterface()->AddClass(&(get_class_descriptor())) != -1);
    }

    if(!loading)
    {
        get_class_descriptor().MakeAutoParamBlocks(this);
    }
}

EnvironmentContainer::~EnvironmentContainer()
{

}

ClassDesc2& EnvironmentContainer::get_class_descriptor()
{
    static ClassDescriptor class_desc;
    return class_desc;
}

EnvironmentContainer::BackgroundMode EnvironmentContainer::GetBackgroundMode() const 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        Interval dummy_interval;    // not animated
        return static_cast<BackgroundMode>(m_param_block->GetInt(ParamID_BackgroundMode, 0, dummy_interval));
    }
    else
    {
        return BackgroundMode::Environment;
    }
}

void EnvironmentContainer::SetBackgroundMode(const BackgroundMode mode) 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        DbgVerify(m_param_block->SetValue(ParamID_BackgroundMode, 0, static_cast<int>(mode)));
    }
}

Texmap* EnvironmentContainer::GetBackgroundTexture() const 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        Interval dummy_interval;
        return m_param_block->GetTexmap(ParamID_BackgroundTexture, 0, dummy_interval);
    }
    else
    {
        return nullptr;
    }
}

void EnvironmentContainer::SetBackgroundTexture(Texmap* const texture) 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        DbgVerify(m_param_block->SetValue(ParamID_BackgroundTexture, 0, texture));
    }
}

AColor EnvironmentContainer::GetBackgroundColor(const TimeValue t, Interval& validity) const 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        return m_param_block->GetAColor(ParamID_BackgroundColor, t, validity);
    }
    else
    {
        return AColor(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

void EnvironmentContainer::SetBackgroundColor(const TimeValue t, const AColor color) 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        DbgVerify(m_param_block->SetValue(ParamID_BackgroundColor, t, color));
    }
}

EnvironmentContainer::EnvironmentMode EnvironmentContainer::GetEnvironmentMode() const 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        Interval dummy_interval;
        return static_cast<EnvironmentMode>(m_param_block->GetInt(ParamID_EnvironmentMode, 0, dummy_interval));
    }
    else
    {
        return EnvironmentMode::None;
    }
}

void EnvironmentContainer::SetEnvironmentMode(const EnvironmentMode mode) 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        DbgVerify(m_param_block->SetValue(ParamID_EnvironmentMode, 0, static_cast<int>(mode)));
    }
}

Texmap* EnvironmentContainer::GetEnvironmentTexture() const 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        Interval dummy_interval;
        return m_param_block->GetTexmap(ParamID_EnvironmentTexture, 0, dummy_interval);
    }
    else
    {
        return nullptr;
    }
}

void EnvironmentContainer::SetEnvironmentTexture(Texmap* const texture) 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        DbgVerify(m_param_block->SetValue(ParamID_EnvironmentTexture, 0, texture));
    }
}

AColor EnvironmentContainer::GetEnvironmentColor(const TimeValue t, Interval& validity) const 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        return m_param_block->GetAColor(ParamID_EnvironmentColor, t, validity);
    }
    else
    {
        return AColor(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

void EnvironmentContainer::SetEnvironmentColor(const TimeValue t, const AColor color) 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        DbgVerify(m_param_block->SetValue(ParamID_EnvironmentColor, t, color));
    }
}

AColor EnvironmentContainer::EvalColor(ShadeContext& /*sc*/) 
{
    //!! TODO: Discriminate against ray type, and correctly evaluate the env or background.
    return AColor(0.0f, 0.0f, 0.0f, 0.0f);
}

Point3 EnvironmentContainer::EvalNormalPerturb(ShadeContext& /*sc*/)
{
    // No support for bump mapping with environment maps
    return Point3(0.0f, 0.0f, 0.0f);
}

void EnvironmentContainer::Update(TimeValue t, Interval& valid) 
{
    InstallLegacyEnvironment();

    // Query the validity of all parameters
    if(DbgVerify(m_param_block != nullptr))
    {
        m_param_block->GetValidity(t, valid);
    }
}

void EnvironmentContainer::Reset() 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        m_param_block->ResetAll();
    }
}

Interval EnvironmentContainer::Validity(TimeValue t) 
{
    if(DbgVerify(m_param_block != nullptr))
    {
        Interval validity = FOREVER;
        m_param_block->GetValidity(t, validity);
        return validity;
    }
    else
    {
        return FOREVER;
    }
}

ParamDlg* EnvironmentContainer::CreateParamDlg(HWND /*hwMtlEdit*/, IMtlParams* /*imp*/) 
{
    // No dialog for the moment
    return nullptr;
}

RefTargetHandle	EnvironmentContainer::Clone( RemapDir	&remap )
{
    EnvironmentContainer* clone = new EnvironmentContainer(true);
    if(DbgVerify(clone != nullptr))
    {
        const int num_refs = NumRefs();
        for(int i = 0; i < num_refs; ++i)
        {
            clone->ReplaceReference(i, remap.CloneRef(GetReference(i))); 
        }
        BaseClone(this, clone, remap);
    }

    return clone;
}

RefResult EnvironmentContainer::NotifyRefChanged(const Interval& /*changeInt*/, RefTargetHandle /*hTarget*/, PartID& /*partID*/, RefMessage /*message*/, BOOL /*propagate*/)
{
    return REF_SUCCEED;
}

IOResult EnvironmentContainer::Load(ILoad* iload)
{
    // Load MtlBase data
    return __super::Load(iload);
}

IOResult EnvironmentContainer::Save(ISave* isave)
{
    // Save MtlBase data
    return __super::Save(isave);

}

int EnvironmentContainer::NumRefs()
{
    return 2;
}

RefTargetHandle	EnvironmentContainer::GetReference(int i)
{
    switch(i)
    {
    case 0:
        return m_param_block;
    case 1:
        return m_legacy_environment;
    default:
        DbgAssert(false);
        return nullptr;
    }
}

void EnvironmentContainer::SetReference(int	i, RefTargetHandle rtarg)
{
    switch(i)
    {
    case 0:
        m_param_block = dynamic_cast<IParamBlock2*>(rtarg);
        DbgAssert(rtarg == m_param_block);
        break;
    case 1:
        m_legacy_environment = dynamic_cast<Texmap*>(rtarg);
        DbgAssert(rtarg == m_legacy_environment);
        break;
    default:
        DbgAssert(false);
        break;
    }
}

Class_ID EnvironmentContainer::ClassID()
{
    return get_class_descriptor().ClassID();
}

void EnvironmentContainer::GetClassName(TSTR& s)
{
    s = get_class_descriptor().ClassName();
}

int EnvironmentContainer::NumSubs()
{
    return 1;
}

TSTR EnvironmentContainer::SubAnimName(int i)
{
    switch(i)
    {
    case 0:
        return (m_param_block != nullptr) ? m_param_block->GetLocalName() : _T("");    
    default:
        DbgAssert(false);
        return _T("");
    }
}

Animatable*	EnvironmentContainer::SubAnim(int	i)
{
    switch(i)
    {
    case 0:
        return m_param_block;
    default:
        DbgAssert(false);
        return nullptr;
    }
}

int EnvironmentContainer::NumParamBlocks()
{
    return 1;
}

IParamBlock2* EnvironmentContainer::GetParamBlock(int i)
{
    switch(i)
    {
    case 0:
        return m_param_block;
    default:
        DbgAssert(false);
        return nullptr;
    }
}

IParamBlock2* EnvironmentContainer::GetParamBlockByID(BlockID id)
{
    switch(id)
    {
    case 0:
        return m_param_block;
    default:
        DbgAssert(false);
        return nullptr;
    }
}

void EnvironmentContainer::DeleteThis()
{
    delete this;
}

void EnvironmentContainer::SetLegacyEnvironment(Texmap* const env_tex, const Color background_color)
{
    m_use_legacy_environment = true;
    ReplaceReference(1, env_tex);
    m_legacy_background_color = background_color;

    InstallLegacyEnvironment();
}

void EnvironmentContainer::InstallLegacyEnvironment()
{
    if(m_use_legacy_environment && DbgVerify(m_param_block != nullptr))
    {
        // Suspend animation while setting up parameters
        AnimationSuspendGuard animation_suspend_guard;

        // Clear any animation controls on all parameters
        const int num_params = m_param_block->NumParams();
        for(int i = 0; i < num_params; ++i)
        {
            if(m_param_block->GetControllerByIndex(i) != nullptr)
            {
                m_param_block->RemoveControllerByIndex(i, 0);
            }
        }
        
        // Check if the environment provided is a mental ray environment/background switcher
        if((m_legacy_environment != nullptr) && is_mr_switcher_env(*m_legacy_environment))
        {
            IParamBlock2* const params_pb = find_paramblock_by_name(*m_legacy_environment, _T("mip_rayswitch_environment Parameters"));
            IParamBlock2* const connections_pb = find_paramblock_by_name(*m_legacy_environment, _T("mip_rayswitch_environment Connections"));
            if(DbgVerify((params_pb != nullptr) && (connections_pb != nullptr)))
            {             
                const ParamID bg_color_param_id = params_pb->GetDesc()->NameToIndex(_T("background"));
                const ParamID env_color_param_id = params_pb->GetDesc()->NameToIndex(_T("environment"));
                const ParamID bg_map_enabled_param_id = connections_pb->GetDesc()->NameToIndex(_T("background.connected"));
                const ParamID bg_map_param_id = connections_pb->GetDesc()->NameToIndex(_T("background.shader"));
                const ParamID env_map_enabled_param_id = connections_pb->GetDesc()->NameToIndex(_T("environment.connected"));
                const ParamID env_map_param_id = connections_pb->GetDesc()->NameToIndex(_T("environment.shader"));

                // Copy the environment and background colors, along with their animation controllers
                m_param_block->Assign(ParamID_BackgroundColor, params_pb, bg_color_param_id);
                m_param_block->Assign(ParamID_EnvironmentColor, params_pb, env_color_param_id);

                // Setup the texture maps
                Interval dummy_interval;        // all these parameters are not animated
                const bool bg_tex_enabled = !!connections_pb->GetInt(bg_map_enabled_param_id, 0, dummy_interval);
                Texmap* const bg_tex = bg_tex_enabled ? connections_pb->GetTexmap(bg_map_param_id, 0, dummy_interval) : nullptr;
                SetBackgroundMode((bg_tex != nullptr) ? BackgroundMode::Texture : BackgroundMode::Color);
                SetBackgroundTexture(bg_tex);

                const bool env_tex_enabled = !!connections_pb->GetInt(env_map_enabled_param_id, 0, dummy_interval);
                Texmap* const legacy_environment = env_tex_enabled ? connections_pb->GetTexmap(env_map_param_id, 0, dummy_interval) : nullptr;
                SetEnvironmentMode((legacy_environment != nullptr) ? EnvironmentMode::Texture : EnvironmentMode::Color);
                SetEnvironmentTexture(legacy_environment);
            }
        }
        else
        {
            // Set env/background color
            SetBackgroundColor(0, m_legacy_background_color);
            SetEnvironmentColor(0, m_legacy_background_color);

            // Setup env/background texture
            if(m_legacy_environment != nullptr)
            {
                if(uses_screen_mapping(*m_legacy_environment))
                {
                    // Screen mapped texture goes into background slot
                    SetBackgroundTexture(m_legacy_environment);
                    SetBackgroundMode(BackgroundMode::Texture);

                    // No environment
                    SetEnvironmentTexture(nullptr);
                    SetEnvironmentMode(EnvironmentMode::None);
                }
                else
                {
                    // Background uses environment
                    SetBackgroundTexture(nullptr);
                    SetBackgroundMode(BackgroundMode::Environment);

                    // Environment-mapped texture goes into environment slot
                    SetEnvironmentTexture(m_legacy_environment);
                    SetEnvironmentMode(EnvironmentMode::Texture);
                }
            }
            else
            {
                // No environment or background map: background color for both background and environment
                SetEnvironmentTexture(nullptr);
                SetBackgroundMode(BackgroundMode::Environment);
                SetBackgroundTexture(nullptr);
                SetEnvironmentMode(EnvironmentMode::Color);
            }
        }
    }
}

//==================================================================================================
// class EnvironmentContainer::ClassDescriptor
//==================================================================================================

const MCHAR* EnvironmentContainer::ClassDescriptor::ClassName() 
{
    static const MSTR str = MaxSDK::GetResourceStringAsMSTR(IDS_ENVIRONMENT_CONTAINER_CLASSNAME);
    return str;
}

Class_ID EnvironmentContainer::ClassDescriptor::ClassID() 
{
    return Class_ID(0x5d86323, 0x3cbc3134);
}

int	EnvironmentContainer::ClassDescriptor::IsPublic() 
{
    // Don't expose publicly - this class only exists for internal purposes
    return false;
}

void* EnvironmentContainer::ClassDescriptor::Create(BOOL loading) 
{
    Texmap* const env = new EnvironmentContainer(!!loading);
    return env;
}

SClass_ID EnvironmentContainer::ClassDescriptor::SuperClassID() 
{
    return TEXMAP_CLASS_ID;
}

const TCHAR* EnvironmentContainer::ClassDescriptor::Category() 
{
    return _T("");
}

const TCHAR* EnvironmentContainer::ClassDescriptor::InternalName() 
{
    return _T("EnvironmentContainer");
}

HINSTANCE EnvironmentContainer::ClassDescriptor::HInstance() 
{
    return MaxSDK::GetHInstance();
}

}}	// namespace

