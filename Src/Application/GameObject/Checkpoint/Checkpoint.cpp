#include "../../../Pch.h"
#include "Checkpoint.h"

void Checkpoint::Init()
{
	m_pDebugWire = std::make_unique<KdDebugWireFrame>();
}

void Checkpoint::Update()
{
	const auto spPlayer = m_wpPlayer.lock();
	if (!spPlayer) { return; }

	const Math::Vector3 toPlayer = spPlayer->GetPos() - GetPos();
	const float distSq = toPlayer.LengthSquared();
	const float r      = CheckpointConst::TriggerRadius;

	if (distSq <= r * r)
	{
		m_activated = true;
	}
}

void Checkpoint::DrawDebug()
{
	if (!m_pDebugWire) { return; }

	m_pDebugWire->AddDebugSphere(
		GetPos(),
		CheckpointConst::TriggerRadius,
		{ CheckpointConst::DebugColorR,
		  CheckpointConst::DebugColorG,
		  CheckpointConst::DebugColorB,
		  CheckpointConst::DebugColorA });
	m_pDebugWire->Draw();
}
