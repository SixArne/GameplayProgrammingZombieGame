#pragma once

class IDecisionMaking
{
public:
	IDecisionMaking() = default;
	virtual ~IDecisionMaking() = default;

	virtual void Update(float deltaT) = 0;

};

#include "EBlackboard.h"
#include "EBehaviorTree.h"