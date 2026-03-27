#include "Character/ActCharacterMovementNotifies.h"

#include "Character/ActCharacter.h"
#include "Character/ActCharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_CancelWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (const AActCharacter* Character = MeshComp ? Cast<AActCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		if (UActCharacterMovementComponent* MovementComponent = Character->GetActMovementComponent())
		{
			MovementComponent->PushCancelWindow(this);
		}
	}
}

void UANS_CancelWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (const AActCharacter* Character = MeshComp ? Cast<AActCharacter>(MeshComp->GetOwner()) : nullptr)
	{
		if (UActCharacterMovementComponent* MovementComponent = Character->GetActMovementComponent())
		{
			MovementComponent->PopCancelWindow(this);
		}
	}
}
