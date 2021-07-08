// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include <Cry3DEngine/IBreezeGenerator.h>
#include <CrySystem/IStreamEngine.h>

class CFlowNode_EnvMoonDirection : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_EnvMoonDirection(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Get = 0,
		eIP_Set,
		eIP_Latitude,
		eIP_Longitude,
		eIP_ForceUpdate,
	};

	enum EOutputPorts
	{
		eOP_Latitude = 0,
		eOP_Longitude,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get",          _HELP("Get the current Latitude/Longitude")),
			InputPortConfig_Void("Set",          _HELP("Set the Latitude/Longitude")),
			InputPortConfig<float>("Latitude",   0.0f,                                        _HELP("Latitude to be set")),
			InputPortConfig<float>("Longitude",  35.0f,                                       _HELP("Longitude to be set")),
			InputPortConfig<bool>("ForceUpdate", false,                                       _HELP("Force Immediate Update of Sky if true. USE ONLY IN SPECIAL CASES, is heavy on performance!")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("Latitude",  _HELP("Current Latitude")),
			OutputPortConfig<float>("Longitude", _HELP("Current Longitude")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Set the 3DEngine's Moon Direction");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Get))
			{
				Vec3 v;
				gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
				ActivateOutput(pActInfo, eOP_Latitude, v.x);
				ActivateOutput(pActInfo, eOP_Longitude, v.y);
			}
			if (IsPortActive(pActInfo, eIP_Set))
			{
				Vec3 v(ZERO);
				v.x = GetPortFloat(pActInfo, eIP_Latitude);
				v.y = GetPortFloat(pActInfo, eIP_Longitude);
				gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
				ActivateOutput(pActInfo, eOP_Latitude, v.x);
				ActivateOutput(pActInfo, eOP_Longitude, v.y);
				bool forceUpdate = GetPortBool(pActInfo, eIP_ForceUpdate);
				ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
				if (forceUpdate && pTOD)
					pTOD->Update(true, true);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

class CFlowNode_EnvSun : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_EnvSun(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Get = 0,
		eIP_Set,
		eIP_Latitude,
		eIP_Longitude,
		eIP_ForceUpdate,
	};

	enum EOutputPorts
	{
		eOP_Latitude = 0,
		eOP_Longitude,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get",          _HELP("Get the current Latitude/Longitude")),
			InputPortConfig_Void("Set",          _HELP("Set the Latitude/Longitude")),
			InputPortConfig<float>("Latitude",   0.0f,                                        _HELP("Latitude to be set")),
			InputPortConfig<float>("Longitude",  35.0f,                                       _HELP("Longitude to be set")),
			InputPortConfig<bool>("ForceUpdate", false,                                       _HELP("Force Immediate Update of Sky if true. USE ONLY IN SPECIAL CASES, is heavy on performance!")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("Latitude",  _HELP("Current Latitude")),
			OutputPortConfig<float>("Longitude", _HELP("Current Longitude")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Set the 3DEngine's Sun Data");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Get))
			{
				ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
				if (pTOD)
				{
					ActivateOutput(pActInfo, eOP_Latitude, pTOD->GetSunParams().latitude);
					ActivateOutput(pActInfo, eOP_Longitude, pTOD->GetSunParams().longitude);
				}
			}
			if (IsPortActive(pActInfo, eIP_Set))
			{
				ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
				if (pTOD)
				{
					float latitude = GetPortFloat(pActInfo, eIP_Latitude);
					float longitude = GetPortFloat(pActInfo, eIP_Longitude);

					pTOD->GetSunParams().longitude = longitude;
					pTOD->GetSunParams().latitude = latitude;
					bool forceUpdate = GetPortBool(pActInfo, eIP_ForceUpdate);
					pTOD->Update(true, forceUpdate);

					ActivateOutput(pActInfo, eOP_Latitude, latitude);
					ActivateOutput(pActInfo, eOP_Longitude, longitude);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_TimeOfDayTransitionTrigger : public CFlowBaseNode<eNCT_Instanced>
{
	typedef uint32 InternalID;  // InternalID is used to control that only 1 of those nodes is active at any time

	float             m_startTOD;
	float             m_startSunLatitude;
	float             m_startSunLongitude;
	float             m_durationLeft;
	float             m_TODTimeToUpdate; // used when update is not every frame
	float             m_sunTimeToUpdate; // used when update is not every frame
	InternalID        m_ID;
	bool              m_blending;
	bool              m_paused;

	static InternalID m_IDCounter;
	static InternalID m_activeID;

public:
	enum EInputs
	{
		IN_TOD = 0,
		IN_DURATION,
		IN_SUN_LATITUDE,
		IN_SUN_LONGITUDE,
		IN_SUN_POSITION_UPDATE_INTERVAL,
		IN_TOD_FORCE_UPDATE_INTERVAL,
		IN_START,
		IN_PAUSE
	};
	enum EOutputs
	{
		OUT_DONE = 0,
	};

	CFlowNode_TimeOfDayTransitionTrigger(SActivationInfo* pActInfo)
		: m_startTOD(0.0f)
		, m_startSunLatitude(0.0f)
		, m_startSunLongitude(0.0f)
		, m_durationLeft(0.0f)
		, m_TODTimeToUpdate(0.0f)
		, m_sunTimeToUpdate(0.0f)
		, m_ID(m_IDCounter++)
		, m_blending(false)
		, m_paused(false)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_TimeOfDayTransitionTrigger(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("blending", m_blending);
		ser.Value("IDCounter", m_IDCounter); // those statics will get serialized more than once (once for each node), but does not matter much
		ser.Value("activeID", m_activeID);
		if (m_blending)
		{
			ser.Value("paused", m_paused);
			ser.Value("startTOD", m_startTOD);
			ser.Value("startSunLatitude", m_startSunLatitude);
			ser.Value("startSunLongitude", m_startSunLongitude);
			ser.Value("durationLeft", m_durationLeft);
			ser.Value("TODTimeToUpdate", m_TODTimeToUpdate);
			ser.Value("sunTimeToUpdate", m_sunTimeToUpdate);
		}
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("Time",                      -1.0f,                                  _HELP("Blend from the current time in the level to this new specified time. (24 hours value. -1 = no TOD blending)")),
			InputPortConfig<float>("Duration",                  0.f,                                    _HELP("Blend duration in seconds")),
			InputPortConfig<float>("SunLatitude",               -1.f,                                   _HELP("Blend from the current sun latitude in the level to this new specified latitude (value in degrees. -1=no latitude blending)")),
			InputPortConfig<float>("SunLongitude",              -1.f,                                   _HELP("blend from the current sun longitude in the level to this new specified longitude (value in degrees. -1=no longitude blending)")),
			InputPortConfig<float>("SunPositionUpdateInterval", 1.f,                                    _HELP("Amount of time in seconds between every update for the repositioning of the sun (0 second means the sun position is constantly updated during the transition) ")),
			InputPortConfig<float>("ForceUpdateInterval",       1.0f,                                   _HELP("amount of time in seconds between every update of the time of day (0 second means the time of day is constantly updated during the transition, not recommended for low spec) ")),
			InputPortConfig_Void("Start",                       _HELP("Starts the transition")),
			InputPortConfig_Void("Pause",                       _HELP("Pause/Unpause the transition")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Done", _HELP("triggers when transition is finished")),
			{ 0 }
		};

		config.sDescription = _HELP("Time of Day and sun position transitions.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_blending = false;
				m_paused = false;
				break;
			}

		case eFE_Activate:
			{
				ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
				if (!pTOD)
					break;

				if (IsPortActive(pActInfo, IN_START))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					m_durationLeft = GetPortFloat(pActInfo, IN_DURATION);
					m_sunTimeToUpdate = GetPortFloat(pActInfo, IN_SUN_POSITION_UPDATE_INTERVAL);
					m_TODTimeToUpdate = GetPortFloat(pActInfo, IN_TOD_FORCE_UPDATE_INTERVAL);
					m_startSunLongitude = pTOD->GetSunParams().longitude;
					m_startSunLatitude = pTOD->GetSunParams().latitude;
					m_startTOD = pTOD->GetTime();
					m_blending = true;
					m_paused = false;
					m_activeID = m_ID;
				}
				if (IsPortActive(pActInfo, IN_PAUSE))
				{
					if (m_blending)
					{
						m_paused = !m_paused;
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, !m_paused);
					}
				}
				break;
			}

		case eFE_Update:
			{
				if (m_activeID != m_ID)
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					m_blending = false;
					break;
				}

				ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
				if (!pTOD)
					break;

				m_durationLeft -= gEnv->pTimer->GetFrameTime();
				bool forceUpdate = false;
				if (m_durationLeft <= 0)
				{
					m_durationLeft = 0;
					m_blending = false;
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, OUT_DONE, true);
					forceUpdate = true;
					m_activeID = 0xffffffff;
				}
				float totalDuration = GetPortFloat(pActInfo, IN_DURATION);
				float blendPos = totalDuration == 0 ? 1.f : 1.f - (m_durationLeft / totalDuration);

				bool needUpdate = UpdateTOD(blendPos, pActInfo, pTOD, forceUpdate);
				needUpdate |= UpdateSun(blendPos, pActInfo, pTOD, forceUpdate);

				if (needUpdate)
					pTOD->Update(true, true);
				break;
			}
		}
	}

	bool UpdateTOD(float blendPos, SActivationInfo* pActInfo, ITimeOfDay* pTOD, bool forceUpdate)
	{
		bool needUpdate = forceUpdate;
		float endTOD = GetPortFloat(pActInfo, IN_TOD);
		if (endTOD < 0)
			return needUpdate;

		m_TODTimeToUpdate -= gEnv->pTimer->GetFrameTime();
		if (m_TODTimeToUpdate <= 0 || forceUpdate)
		{
			m_TODTimeToUpdate = GetPortFloat(pActInfo, IN_TOD_FORCE_UPDATE_INTERVAL);
			float currTime = m_startTOD + (endTOD - m_startTOD) * blendPos;
			pTOD->SetTime(currTime, false);
			needUpdate = true;
		}
		return needUpdate;
	}

	bool UpdateSun(float blendPos, SActivationInfo* pActInfo, ITimeOfDay* pTOD, bool forceUpdate)
	{
		bool needUpdate = forceUpdate;
		m_sunTimeToUpdate -= gEnv->pTimer->GetFrameTime();
		if (m_sunTimeToUpdate > 0 && !forceUpdate)
			return needUpdate;

		m_sunTimeToUpdate = GetPortFloat(pActInfo, IN_SUN_POSITION_UPDATE_INTERVAL);

		float endLatitude = GetPortFloat(pActInfo, IN_SUN_LATITUDE);
		float currLatitude = pTOD->GetSunParams().latitude;
		if (endLatitude >= 0)
		{
			currLatitude = m_startSunLatitude + (endLatitude - m_startSunLatitude) * blendPos;
			needUpdate = true;
		}

		float endLongitude = GetPortFloat(pActInfo, IN_SUN_LONGITUDE);
		float currLongitude = pTOD->GetSunParams().longitude;
		if (endLongitude >= 0)
		{
			currLongitude = m_startSunLongitude + (endLongitude - m_startSunLongitude) * blendPos;
			needUpdate = true;
		}

		if (needUpdate)
		{
			pTOD->GetSunParams().longitude = currLongitude;
			pTOD->GetSunParams().latitude = currLatitude;
		}

		return needUpdate;
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

CFlowNode_TimeOfDayTransitionTrigger::InternalID CFlowNode_TimeOfDayTransitionTrigger::m_IDCounter = 0;
CFlowNode_TimeOfDayTransitionTrigger::InternalID CFlowNode_TimeOfDayTransitionTrigger::m_activeID = 0xffffffff;

//////////////////////////////////////////////////////////////////////////
class CFlowNode_EnvWind : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_EnvWind(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Get = 0,
		eIP_Set,
		eIP_WindVector,
		eIP_BreezeEnabled,
		eIP_BreezeStrength,
		eIP_BreezeMovementSpeed,
		eIP_BreezeVariance,
		eIP_BreezeLifetime,
		eIP_BreezeCount,
		eIP_BreezeSpawnRadius,
		eIP_BreezeSpread,
		eIP_BreezeRadius,
		eIP_BreezeAwakeThreshold,
		eIP_BreezeFixedHeight
	};

	enum EOutputPorts
	{
		eOP_WindVector,
		eOP_BreezeEnabled,
		eOP_BreezeStrength,
		eOP_BreezeMovementSpeed,
		eOP_BreezeVariance,
		eOP_BreezeLifetime,
		eOP_BreezeCount,
		eOP_BreezeSpawnRadius,
		eOP_BreezeSpread,
		eOP_BreezeRadius,
		eOP_BreezeAwakeThreshold,
		eOP_BreezeFixedHeight
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get",                    _HELP("Get Wind Parameters")),
			InputPortConfig_Void("Set",                    _HELP("Set Wind Parameters")),
			InputPortConfig<Vec3>("WindVector",            Vec3(1.f, 0.f, 0.f),          _HELP("Environment Wind Vector")),
			InputPortConfig<bool>("BreezeGeneration",      false,                        _HELP("Breeze Generation Enabled")),
			InputPortConfig<float>("BreezeStrength",       1.f,                          _HELP("Breeze Strength")),
			InputPortConfig<float>("BreezeMovementSpeed",  8.f,                          _HELP("Breeze Movement Speed")),
			InputPortConfig<float>("BreezeVariance",       1.f,                          _HELP("Breeze Variation")),
			InputPortConfig<float>("BreezeLifeTime",       15.f,                         _HELP("Breeze Life Time")),
			InputPortConfig<int>("BreezeCount",            4,                            _HELP("Breeze Count")),
			InputPortConfig<float>("BreezeSpawnRadius",    25.f,                         _HELP("Breeze Spawn Radius")),
			InputPortConfig<float>("BreezeSpread",         0.f,                          _HELP("Breeze Spread")),
			InputPortConfig<float>("BreezeRadius",         5.f,                          _HELP("Breeze Radius")),
			InputPortConfig<float>("BreezeAwakeThreshold", 0.f,                          _HELP("Breeze Awake Threshold")),
			InputPortConfig<float>("FixedHeight",          -1.f,                         _HELP("Breeze Height. If -1, use terrain height")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("WindVector",                                          _HELP("Current Environment Wind Vector")),
			OutputPortConfig<bool>("BreezeGeneration",                                    _HELP("Breeze Generation Enabled")),
			OutputPortConfig<float>("BreezeStrength",                                     _HELP("Breeze Strength")),
			OutputPortConfig<float>("BreezeMovementSpeed",                                _HELP("Breeze Movement Speed")),
			OutputPortConfig<float>("BreezeVariance",                                     _HELP("Breeze Variation")),
			OutputPortConfig<float>("BreezeLifeTime",                                     _HELP("Breeze Life Time")),
			OutputPortConfig<int>("BreezeCount",                                          _HELP("Breeze Count")),
			OutputPortConfig<float>("BreezeSpawnRadius",                                  _HELP("Breeze Spawn Radius")),
			OutputPortConfig<float>("BreezeSpread",                                       _HELP("Breeze Spread")),
			OutputPortConfig<float>("BreezeRadius",                                       _HELP("Breeze Radius")),
			OutputPortConfig<float>("BreezeAwakeThreshold",                               _HELP("Breeze Awake Threshold")),
			OutputPortConfig<float>("FixedHeight",                                        _HELP("Breeze Height. If -1, use terrain height")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Access 3DEngine's Wind Parameters");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Get))
			{
				ActivateOutput(pActInfo, eOP_WindVector, gEnv->p3DEngine->GetGlobalWind(false));

				if (const IBreezeGenerator* pBreezeGenerator = gEnv->p3DEngine->GetBreezeGenerator())
				{
					const BreezeGeneratorParams params = pBreezeGenerator->GetParams();
					
					ActivateOutput(pActInfo, eOP_BreezeEnabled,        params.breezeGenerationEnabled);
					ActivateOutput(pActInfo, eOP_BreezeStrength,       params.breezeStrength);
					ActivateOutput(pActInfo, eOP_BreezeMovementSpeed,  params.breezeMovementSpeed);
					ActivateOutput(pActInfo, eOP_BreezeVariance,       params.breezeVariance);
					ActivateOutput(pActInfo, eOP_BreezeLifetime,       params.breezeLifeTime);
					ActivateOutput(pActInfo, eOP_BreezeCount,          params.breezeCount);
					ActivateOutput(pActInfo, eOP_BreezeSpawnRadius,    params.breezeSpawnRadius);
					ActivateOutput(pActInfo, eOP_BreezeSpread,         params.breezeSpread);
					ActivateOutput(pActInfo, eOP_BreezeRadius,         params.breezeRadius);
					ActivateOutput(pActInfo, eOP_BreezeAwakeThreshold, params.breezeAwakeThreshold);
					ActivateOutput(pActInfo, eOP_BreezeFixedHeight,    params.breezeFixedHeight);
				}
			}
			if (IsPortActive(pActInfo, eIP_Set))
			{
				const Vec3 wind = GetPortVec3(pActInfo, eIP_WindVector);
				gEnv->p3DEngine->SetWind(wind);

				if (IBreezeGenerator* pBreezeGenerator = gEnv->p3DEngine->GetBreezeGenerator())
				{
					BreezeGeneratorParams params = pBreezeGenerator->GetParams();

					params.windVector = wind;
					params.breezeGenerationEnabled = GetPortBool(pActInfo, eIP_BreezeEnabled);
					params.breezeStrength          = GetPortFloat(pActInfo, eIP_BreezeStrength);
					params.breezeVariance          = GetPortFloat(pActInfo, eIP_BreezeVariance);
					params.breezeLifeTime          = GetPortFloat(pActInfo, eIP_BreezeLifetime);

					const int breezeCount          = GetPortInt(pActInfo, eIP_BreezeCount);
					params.breezeCount = breezeCount > 0 ? breezeCount : 0;

					params.breezeRadius            = GetPortFloat(pActInfo, eIP_BreezeRadius);
					params.breezeSpawnRadius       = GetPortFloat(pActInfo, eIP_BreezeSpawnRadius);
					params.breezeSpread            = GetPortFloat(pActInfo, eIP_BreezeSpread);
					params.breezeMovementSpeed     = GetPortFloat(pActInfo, eIP_BreezeMovementSpeed);
					params.breezeAwakeThreshold    = GetPortFloat(pActInfo, eIP_BreezeAwakeThreshold);
					params.breezeFixedHeight       = GetPortFloat(pActInfo, eIP_BreezeFixedHeight);
					
					pBreezeGenerator->SetParams(params);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_SetOceanMat : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SetOceanMat(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_Set = 0,
		eIP_Material,
	};

	enum EOutputPorts
	{
		eOP_Success = 0,
		eOP_Failed,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Set",         _HELP("Sets the material on the ocean")),
			InputPortConfig<string>("Material", _HELP("Material of choice to be set"),   0,_UICONFIG("dt=mat")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Success", _HELP("Triggered when successfully finished")),
			OutputPortConfig_Void("Failed",  _HELP("Triggered when something went wrong")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Set the ocean material");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_Set))
			{
				string mat = GetPortString(pActInfo, eIP_Material);
				IMaterial* pMat = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(mat.c_str(), false);
				if (pMat)
				{
					gEnv->p3DEngine->GetITerrain()->ChangeOceanMaterial(pMat);
					ActivateOutput(pActInfo, eOP_Success, 0);
				}
				else
				{
					ActivateOutput(pActInfo, eOP_Failed, 0);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

class CFlowNode_SkyMaterialSwitch : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SkyMaterialSwitch(SActivationInfo* pActInfo){}
	virtual void GetMemoryUsage(ICrySizer* s) const;
	void         GetConfiguration(SFlowNodeConfig& config);
	void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
};

void CFlowNode_SkyMaterialSwitch::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
}

void CFlowNode_SkyMaterialSwitch::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig<string>("hdr_mat_Material",    _HELP("Skybox material name (default-spec)")),
		InputPortConfig<string>("mat_Material",        _HELP("Skybox material name (low-spec)")),
		InputPortConfig<bool>("Start",          false, _HELP("Trigger to start the loading")),
		InputPortConfig<float>("Angle",         1.f,   _HELP("Skybox rotation")),
		InputPortConfig<float>("Stretching",    1.f,   _HELP("Skybox stretching")),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.sDescription = _HELP("Node for sky material switching");
	config.SetCategory(EFLN_ADVANCED);
}

void CFlowNode_SkyMaterialSwitch::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Activate:
		if (IsPortActive(pActInfo, 2))
		{
			gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_SKYBOX_ANGLE, GetPortFloat(pActInfo, 3));
			gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_SKYBOX_STRETCHING, GetPortFloat(pActInfo, 4));

			const string matNames[2] =
			{
				GetPortString(pActInfo, 1), // eSkyType_Sky
				GetPortString(pActInfo, 0)  // eSkyType_HDRSky
			};

			for (int skyTypeIdx = 0; skyTypeIdx < eSkySpec_NumSkySpecs; ++skyTypeIdx)
			{
				const string matName = matNames[skyTypeIdx];
				IMaterial *const pSkyMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(matNames[skyTypeIdx], false, false, IMaterialManager::ELoadingFlagsPreviewMode);
				gEnv->p3DEngine->SetSkyMaterial(pSkyMtl, (eSkyType) skyTypeIdx);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
class CFlowNode_SetVolumetricCloudSwitch : public CFlowBaseNode<eNCT_Singleton>, public IStreamCallback
{
private:
	const static bool AsyncLoad = true;

	IReadStreamPtr    m_Job;

public:
	enum EInputPorts
	{
		eIP_Texture = 0,
		eIP_TilingSize,
		eIP_TilingOffset,
	};

	enum EOutputPorts
	{
		eOP_Success = 0,
		eOP_Failed,
	};

	CFlowNode_SetVolumetricCloudSwitch(SActivationInfo* pActInfo) : m_Job(nullptr)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("Texture",    "",            _HELP("Set the volumetric cloud texture")),
			InputPortConfig<Vec3>("TilingSize",   Vec3(64000.0f, 64000.0f,                                  4000.0f),_HELP("Set the tiling size of volumetric cloud")),
			InputPortConfig<Vec3>("TilingOffset", Vec3(0.0f,     0.0f,                                      0.0f),  _HELP("Set the tiling offset of volumetric cloud")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Success", _HELP("Triggered when setting the texture successfully finished")),
			OutputPortConfig_Void("Failed",  _HELP("Triggered when setting the texture went wrong")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Set the volumetric cloud texture");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIP_TilingSize))
			{
				gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_TILING_SIZE, GetPortVec3(pActInfo, eIP_TilingSize));
			}
			if (IsPortActive(pActInfo, eIP_TilingOffset))
			{
				gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_VOLCLOUD_TILING_OFFSET, -GetPortVec3(pActInfo, eIP_TilingOffset));
			}
			if (IsPortActive(pActInfo, eIP_Texture) && (m_Job == nullptr))
			{
				bool success = false;
				if (AsyncLoad)
				{
					string szNameTex = GetPortString(pActInfo, eIP_Texture);
					if (szNameTex.empty())
					{
						gEnv->pRenderer->SetVolumetricCloudParams(0);
						success = true;
					}
					else if (gEnv->pCryPak->IsFileExist(szNameTex.c_str()))
					{
						m_Job = gEnv->pSystem->GetStreamEngine()->StartRead(eStreamTaskTypeTexture, szNameTex, this);
						success = true;
					}
					else
					{
						gEnv->pLog->LogError("Volumetric cloud texture not found: %s", szNameTex.c_str());
						success = false;
					}
				}
				else
				{
					int texId = 0;
					string szNameTex = GetPortString(pActInfo, eIP_Texture);
					if (szNameTex.empty())
					{
						gEnv->pRenderer->SetVolumetricCloudParams(0);
						success = true;
					}
					else if (gEnv->pCryPak->IsFileExist(szNameTex.c_str()))
					{
						ITexture* pTex = gEnv->pRenderer->EF_LoadTexture(szNameTex.c_str());
						if (pTex)
						{
							texId = pTex->GetTextureID();
							if (texId > 0)
							{
								gEnv->pRenderer->SetVolumetricCloudParams(texId);
							}
						}

						if (!pTex || texId <= 0)
						{
							gEnv->pLog->LogError("Error loading volumetric cloud texture: %s", szNameTex.c_str());
							success = false;
						}
						else
						{
							success = true;
						}
					}
					else
					{
						gEnv->pLog->LogError("Volumetric cloud texture not found: %s", szNameTex.c_str());
						success = false;
					}
				}

				if (success)
				{
					ActivateOutput(pActInfo, eOP_Success, 0);
				}
				else
				{
					ActivateOutput(pActInfo, eOP_Failed, 0);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError) override
	{
		if (pStream->IsError())
		{
			gEnv->pLog->LogError("Error loading volumetric cloud texture: %s", pStream->GetName());
			CRY_ASSERT(0);
			m_Job = nullptr;
			return;
		}

		int texId = 0;
		const char* pNameTex = pStream->GetName();
		if (pNameTex && gEnv->pCryPak->IsFileExist(pNameTex))
		{
			ITexture* pTex = gEnv->pRenderer->EF_LoadTexture(pNameTex, FT_DONT_STREAM);
			if (pTex)
			{
				texId = pTex->GetTextureID();
				if (texId > 0)
				{
					gEnv->pRenderer->SetVolumetricCloudParams(texId);
				}
			}

			if (!pTex || texId <= 0)
			{
				gEnv->pLog->LogError("Error loading volumetric cloud texture: %s", pNameTex);
			}
		}
		else
		{
			gEnv->pLog->LogError("Volumetric cloud texture not found: %s", pNameTex);
		}

		m_Job = nullptr;
	}
};

class CFlowNode_PresetSwitch : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PresetSwitch(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eIN_Activate = 0,
		eIN_PresetName,
	};

	enum EOutputPorts
	{
		eOP_Success = 0,
		eOP_Failed,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Activate",  _HELP("Input for switching time of day to this preset")),
			InputPortConfig<string>("Preset", _HELP("Preset name")),
			//blend time might be here
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Success", _HELP("Triggered when successfully finished")),
			OutputPortConfig_Void("Failed",  _HELP("Triggered when something went wrong")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Switch environment preset");
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_Activate))
				{
					const string& sPresetName = GetPortString(pActInfo, eIN_PresetName);
					ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
					bool bResult = false;

					if (pTOD)
					{
						bResult = pTOD->SetCurrentPreset(sPresetName.c_str());
					}

					ActivateOutput(pActInfo, bResult ? eOP_Success : eOP_Failed, 0);
				}
			}
			break;
		}
	}
};

class CFlowNode_GetCurrentPreset : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_GetCurrentPreset(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eIP_Get = 0
	};

	enum EOutputPorts
	{
		eOP_PresetName = 0
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get", _HELP("Get the current Preset name")),
			//blend time might be here
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<string>("PresetName", _HELP("Current Preset Name")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Get current preset");
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_Get))
				{
					ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();

					string presetName = (pTOD ? pTOD->GetCurrentPresetName() : "");

					ActivateOutput(pActInfo, eOP_PresetName, presetName);
				}
			}
			break;
		}
	}
};

REGISTER_FLOW_NODE("Environment:MoonDirection", CFlowNode_EnvMoonDirection);
REGISTER_FLOW_NODE("Environment:Sun", CFlowNode_EnvSun);
REGISTER_FLOW_NODE("Time:TimeOfDayTransitionTrigger", CFlowNode_TimeOfDayTransitionTrigger)
REGISTER_FLOW_NODE("Environment:Wind", CFlowNode_EnvWind)
REGISTER_FLOW_NODE("Environment:SetOceanMaterial", CFlowNode_SetOceanMat)
REGISTER_FLOW_NODE("Environment:SkyMaterialSwitch", CFlowNode_SkyMaterialSwitch);
REGISTER_FLOW_NODE("Environment:VolumetericCloudSwitch", CFlowNode_SetVolumetricCloudSwitch);
REGISTER_FLOW_NODE("Environment:PresetSwitch", CFlowNode_PresetSwitch);
REGISTER_FLOW_NODE("Environment:GetCurrentPreset", CFlowNode_GetCurrentPreset);