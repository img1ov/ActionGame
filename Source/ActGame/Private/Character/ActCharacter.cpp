// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ActCharacter.h"

#include "ActGameplayTags.h"
#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "Camera/ActCameraComponent.h"
#include "Camera/ActSpringArmComponent.h"
#include "Character/ActBattleComponent.h"
#include "Character/ActCharacterMovementComponent.h"
#include "Character/ActHealthComponent.h"
#include "Character/ActPawnExtensionComponent.h"
#include "BulletSystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/ActPlayerController.h"
#include "Player/ActPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActCharacter)

static FName NAME_ActCharacterCollisionProfile_Capsule(TEXT("ActPawnCapsule"));
static FName NAME_ActCharacterCollisionProfile_Mesh(TEXT("ActPawnMesh"));

AActCharacter::AActCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UActCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetNetCullDistanceSquared(900000000.f);

	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	check(CapsuleComp);
	CapsuleComp->InitCapsuleSize(30.f, 86.f);
	CapsuleComp->SetCollisionProfileName(NAME_ActCharacterCollisionProfile_Capsule);

	USkeletalMeshComponent* MeshComp = GetMesh();
	MeshComp->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, -88.f));
	MeshComp->SetCollisionProfileName(NAME_ActCharacterCollisionProfile_Mesh);
	
	UActCharacterMovementComponent* ActMoveComp = CastChecked<UActCharacterMovementComponent>(GetCharacterMovement());
	ActMoveComp->bUseControllerDesiredRotation = false;
	ActMoveComp->bOrientRotationToMovement = true;
	ActMoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
	ActMoveComp->GetNavAgentPropertiesRef().bCanCrouch = true;
	ActMoveComp->bCanWalkOffLedgesWhenCrouching = true;
	ActMoveComp->SetCrouchedHalfHeight(65.0f);
	
	PawnExtComponent = CreateDefaultSubobject<UActPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	PawnExtComponent->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemInitialized));
	PawnExtComponent->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemUninitialized));

	BattleComponent = CreateDefaultSubobject<UActBattleComponent>(TEXT("BattleComponent"));

	HealthComponent = CreateDefaultSubobject<UActHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
	HealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::OnDeathFinished);

	BulletSystemComponent = CreateDefaultSubobject<UBulletSystemComponent>(TEXT("BulletSystemComponent"));
	// Bullet simulation is local-only; server/client both run their own BulletWorldSubsystem.
	BulletSystemComponent->SetIsReplicated(false);

	CameraSpringArmComponent = CreateDefaultSubobject<UActSpringArmComponent>(TEXT("CameraSpringArmComponent"));
	CameraSpringArmComponent->SetupAttachment(GetRootComponent());
	CameraSpringArmComponent->SetRelativeLocation(FVector(0, 0, 80.f));
	CameraSpringArmComponent->TargetArmLength = 380.f;
	CameraSpringArmComponent->bEnableCameraLag = true;
	CameraSpringArmComponent->CameraLagSpeed = 15.f;
	CameraSpringArmComponent->CameraRotationLagSpeed = 8.f;
	CameraSpringArmComponent->bUsePawnControlRotation = true;
	
	CameraComponent = CreateDefaultSubobject<UActCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(CameraSpringArmComponent);
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	BaseEyeHeight = 80.f;
	CrouchedEyeHeight = 50.0f;
}

AActPlayerController* AActCharacter::GetActPlayerController() const
{
	return CastChecked<AActPlayerController>(GetController(), ECastCheckedType::NullAllowed);
}

AActPlayerState* AActCharacter::GetActPlayerState() const
{
	return CastChecked<AActPlayerState>(GetPlayerState(), ECastCheckedType::NullAllowed);
}

UActCharacterMovementComponent* AActCharacter::GetActMovementComponent() const
{
	return CastChecked<UActCharacterMovementComponent>(GetCharacterMovement());
}

UActAbilitySystemComponent* AActCharacter::GetActAbilitySystemComponent() const
{
	return Cast<UActAbilitySystemComponent>(GetAbilitySystemComponent());
}

UAbilitySystemComponent* AActCharacter::GetAbilitySystemComponent() const
{
	if (PawnExtComponent == nullptr)
	{
		return nullptr;
	}

	return PawnExtComponent->GetActAbilitySystemComponent();
}

UBulletSystemComponent* AActCharacter::GetBulletSystemComponent_Implementation() const
{
	return BulletSystemComponent;
}

void AActCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		ActASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AActCharacter::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		return ActASC->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

bool AActCharacter::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		return ActASC->HasAllMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool AActCharacter::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		return ActASC->HasAnyMatchingGameplayTags(TagContainer);
	}

	return false;
}

void AActCharacter::ToggleCrouch()
{
	const UActCharacterMovementComponent* ActMoveComp = CastChecked<UActCharacterMovementComponent>(GetCharacterMovement());

	if (IsCrouched() || ActMoveComp->bWantsToCrouch)
	{
		UnCrouch();
	}
	else if (ActMoveComp->IsMovingOnGround())
	{
		Crouch();
	}
}

void AActCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

void AActCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AActCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AActCharacter::Reset()
{
	DisableMovementAndCollision();

	K2_OnReset();

	UninitAndDestroy();
}

void AActCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, ReplicatedAcceleration, COND_SimulatedOnly);
	DOREPLIFETIME(ThisClass, MyTeamID);
}

void AActCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		// Compress Acceleration: XY components as direction + magnitude, Z component as direct value
		const double MaxAccel = MovementComponent->MaxAcceleration;
		const FVector CurrentAccel = MovementComponent->GetCurrentAcceleration();
		double AccelXYRadians, AccelXYMagnitude;
		FMath::CartesianToPolar(CurrentAccel.X, CurrentAccel.Y, AccelXYMagnitude, AccelXYRadians);

		ReplicatedAcceleration.AccelXYRadians   = FMath::FloorToInt((AccelXYRadians / TWO_PI) * 255.0);     // [0, 2PI] -> [0, 255]
		ReplicatedAcceleration.AccelXYMagnitude = FMath::FloorToInt((AccelXYMagnitude / MaxAccel) * 255.0);	// [0, MaxAccel] -> [0, 255]
		ReplicatedAcceleration.AccelZ           = FMath::FloorToInt((CurrentAccel.Z / MaxAccel) * 127.0);   // [-MaxAccel, MaxAccel] -> [-127, 127]
	}
}

void AActCharacter::NotifyControllerChanged()
{
	const FGenericTeamId OldTeamId = GetGenericTeamId();
	
	Super::NotifyControllerChanged();
	
	// Update our team ID based on the controller
	if (HasAuthority() && (GetController() != nullptr))
	{
		if (const IActTeamAgentInterface* ControllerWithTeam = Cast<IActTeamAgentInterface>(GetController()))
		{
			MyTeamID = ControllerWithTeam->GetGenericTeamId();
			ConditionalBroadcastTeamChanged(this, OldTeamId, MyTeamID);
		}
	}
}

void AActCharacter::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	if (GetController() == nullptr)
	{
		if (HasAuthority())
		{
			const FGenericTeamId OldTeamID = MyTeamID;
			MyTeamID = NewTeamID;
			ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
		}
		else
		{
			UE_LOG(LogActTeams, Error, TEXT("You can't set the team ID on a character (%s) except on the authority"), *GetPathNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogActTeams, Error, TEXT("You can't set the team ID on a possessed character (%s); it's driven by the associated controller"), *GetPathNameSafe(this));
	}
}

FGenericTeamId AActCharacter::GetGenericTeamId() const
{
	return MyTeamID;
}

FOnActTeamIndexChangedDelegate* AActCharacter::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

void AActCharacter::FastSharedReplication_Implementation(const FSharedRepMovement& SharedRepMovement)
{
	if (GetWorld()->IsPlayingReplay())
	{
		return;
	}

	// Timestamp is checked to reject old moves.
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		// Timestamp
		SetReplicatedServerLastTransformUpdateTimeStamp(SharedRepMovement.RepTimeStamp);

		// Movement mode
		if (GetReplicatedMovementMode() != SharedRepMovement.RepMovementMode)
		{
			SetReplicatedMovementMode(SharedRepMovement.RepMovementMode);
			GetCharacterMovement()->bNetworkMovementModeChanged = true;
			GetCharacterMovement()->bNetworkUpdateReceived = true;
		}

		// Location, Rotation, Velocity, etc.
		FRepMovement& MutableRepMovement = GetReplicatedMovement_Mutable();
		MutableRepMovement = SharedRepMovement.RepMovement;

		// This also sets LastRepMovement
		OnRep_ReplicatedMovement();

		// Jump force
		SetProxyIsJumpForceApplied(SharedRepMovement.bProxyIsJumpForceApplied);

		// Crouch
		if (IsCrouched() != SharedRepMovement.bIsCrouched)
		{
			SetIsCrouched(SharedRepMovement.bIsCrouched);
			OnRep_IsCrouched();
		}
	}
}

void AActCharacter::MulticastBootstrapAddMove_Implementation(const FActAddMoveParams& Params)
{
	if (HasAuthority())
	{
		return;
	}

	if (UActCharacterMovementComponent* ActMovementComponent = GetActMovementComponent())
	{
		ActMovementComponent->ApplyReplicatedAddMove(Params);
	}
}

void AActCharacter::MulticastStopAddMove_Implementation(const int32 SyncId)
{
	if (HasAuthority())
	{
		return;
	}

	if (UActCharacterMovementComponent* ActMovementComponent = GetActMovementComponent())
	{
		ActMovementComponent->StopReplicatedAddMove(SyncId);
	}
}

void AActCharacter::MulticastPauseAddMove_Implementation(const int32 SyncId)
{
	if (HasAuthority())
	{
		return;
	}

	if (UActCharacterMovementComponent* ActMovementComponent = GetActMovementComponent())
	{
		ActMovementComponent->PauseReplicatedAddMove(SyncId);
	}
}

void AActCharacter::MulticastResumeAddMove_Implementation(const int32 SyncId)
{
	if (HasAuthority())
	{
		return;
	}

	if (UActCharacterMovementComponent* ActMovementComponent = GetActMovementComponent())
	{
		ActMovementComponent->ResumeReplicatedAddMove(SyncId);
	}
}

void AActCharacter::MulticastSetAddMoveRotation_Implementation(const FActAddMoveRotationParams& Params)
{
	if (HasAuthority())
	{
		return;
	}

	if (UActCharacterMovementComponent* ActMovementComponent = GetActMovementComponent())
	{
		ActMovementComponent->ApplyReplicatedAddMoveRotation(Params);
	}
}

void AActCharacter::MulticastClearAddMoveRotation_Implementation()
{
	if (HasAuthority())
	{
		return;
	}

	if (UActCharacterMovementComponent* ActMovementComponent = GetActMovementComponent())
	{
		ActMovementComponent->ClearReplicatedAddMoveRotation();
	}
}

bool AActCharacter::UpdateSharedReplication()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		FSharedRepMovement SharedMovement;
		if (SharedMovement.FillForCharacter(this))
		{
			// Only call FastSharedReplication if data has changed since the last frame.
			// Skipping this call will cause replication to reuse the same bunch that we previously
			// produced, but not send it to clients that already received. (But a new client who has not received
			// it, will get it this frame)
			if (!SharedMovement.Equals(LastSharedReplication, this))
			{
				LastSharedReplication = SharedMovement;
				SetReplicatedMovementMode(SharedMovement.RepMovementMode);

				FastSharedReplication(SharedMovement);
			}
			return true;
		}
	}

	// We cannot fastrep right now. Don't send anything.
	return false;
}

void AActCharacter::OnControllerChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	const FGenericTeamId MyOldTeamID = MyTeamID;
	MyTeamID = IntegerToGenericTeamId(NewTeam);
	ConditionalBroadcastTeamChanged(this, MyOldTeamID, MyTeamID);
}

void AActCharacter::OnRep_MyTeamID(FGenericTeamId OldTeamID)
{
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

void AActCharacter::OnRep_ReplicatedAcceleration()
{
	if (UActCharacterMovementComponent* ActMovementComponent = Cast<UActCharacterMovementComponent>(GetCharacterMovement()))
	{
		// Decompress Acceleration
		const double MaxAccel         = ActMovementComponent->MaxAcceleration;
		const double AccelXYMagnitude = double(ReplicatedAcceleration.AccelXYMagnitude) * MaxAccel / 255.0; // [0, 255] -> [0, MaxAccel]
		const double AccelXYRadians   = double(ReplicatedAcceleration.AccelXYRadians) * TWO_PI / 255.0;     // [0, 255] -> [0, 2PI]

		FVector UnpackedAcceleration(FVector::ZeroVector);
		FMath::PolarToCartesian(AccelXYMagnitude, AccelXYRadians, UnpackedAcceleration.X, UnpackedAcceleration.Y);
		UnpackedAcceleration.Z = double(ReplicatedAcceleration.AccelZ) * MaxAccel / 127.0; // [-127, 127] -> [-MaxAccel, MaxAccel]

		ActMovementComponent->SetReplicatedAcceleration(UnpackedAcceleration);
	}
}

void AActCharacter::OnAbilitySystemInitialized()
{
	UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent();
	check(ActASC);

	HealthComponent->InitializeWithAbilitySystem(ActASC);

	InitializeGameplayTags();
}

void AActCharacter::OnAbilitySystemUninitialized()
{
	HealthComponent->UninitializeFromAbilitySystem();
}

void AActCharacter::PossessedBy(AController* NewController)
{
	const FGenericTeamId OldTeamID = MyTeamID;
	
	Super::PossessedBy(NewController);

	PawnExtComponent->HandleControllerChanged();
	
	// Grab the current team ID and listen for future changes
	if (IActTeamAgentInterface* ControllerAsTeamProvider = Cast<IActTeamAgentInterface>(NewController))
	{
		MyTeamID = ControllerAsTeamProvider->GetGenericTeamId();
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnControllerChangedTeam);
	}
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

void AActCharacter::UnPossessed()
{
	AController* const OldController = GetController();

	// Stop listening for changes from the old controller
	const FGenericTeamId OldTeamID = MyTeamID;
	if (IActTeamAgentInterface* ControllerAsTeamProvider = Cast<IActTeamAgentInterface>(OldController))
	{
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().RemoveAll(this);
	}
	
	Super::UnPossessed();

	PawnExtComponent->HandleControllerChanged();
	
	// Determine what the new team ID should be afterwards
	MyTeamID = DetermineNewTeamAfterPossessionEnds(OldTeamID);
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

void AActCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	PawnExtComponent->HandleControllerChanged();
}

void AActCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	PawnExtComponent->HandlePlayerStateReplicated();
}

void AActCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PawnExtComponent->SetupPlayerInputComponent();
}

void AActCharacter::InitializeGameplayTags()
{
	// Clear tags that may be lingering on the ability system from the previous pawn.
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		for (const TPair<uint8, FGameplayTag>& TagMapping : ActGameplayTags::MovementModeTagMap)
		{
			if (TagMapping.Value.IsValid())
			{
				ActASC->SetLooseGameplayTagCount(TagMapping.Value, 0);
			}
		}

		for (const TPair<uint8, FGameplayTag>& TagMapping : ActGameplayTags::CustomMovementModeTagMap)
		{
			if (TagMapping.Value.IsValid())
			{
				ActASC->SetLooseGameplayTagCount(TagMapping.Value, 0);
			}
		}

		UActCharacterMovementComponent* ActMoveComp = CastChecked<UActCharacterMovementComponent>(GetCharacterMovement());
		SetMovementModeTag(ActMoveComp->MovementMode, ActMoveComp->CustomMovementMode, true);
	}
}

void AActCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	//HealthComponent->DamageSelfDestruct(/*bFellOutOfWorld=*/ true);
	Super::FellOutOfWorld(dmgType);
}

void AActCharacter::OnDeathStarted(AActor* OwningActor)
{
	DisableMovementAndCollision();
}

void AActCharacter::OnDeathFinished(AActor* OwningActor)
{
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::DestroyDueToDeath);
}

void AActCharacter::DisableMovementAndCollision()
{
	if (GetController())
	{
		GetController()->SetIgnoreMoveInput(true);
	}

	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	check(CapsuleComp);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	UActCharacterMovementComponent* ActMoveComp = CastChecked<UActCharacterMovementComponent>(GetCharacterMovement());
	ActMoveComp->StopMovementImmediately();
	ActMoveComp->DisableMovement();
}

void AActCharacter::DestroyDueToDeath()
{
	K2_OnDeathFinished();

	UninitAndDestroy();
}

void AActCharacter::UninitAndDestroy()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		DetachFromControllerPendingDestroy();
		SetLifeSpan(0.1f);
	}

	// Uninitialize the ASC if we're still the avatar actor (otherwise another pawn already did it when they became the avatar actor)
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		if (ActASC->GetAvatarActor() == this)
		{
			PawnExtComponent->UninitializeAbilitySystem();
		}
	}

	SetActorHiddenInGame(true);
}

void AActCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	UActCharacterMovementComponent* ActMoveComp = CastChecked<UActCharacterMovementComponent>(GetCharacterMovement());

	SetMovementModeTag(PrevMovementMode, PreviousCustomMode, false);
	SetMovementModeTag(ActMoveComp->MovementMode, ActMoveComp->CustomMovementMode, true);
}

void AActCharacter::SetMovementModeTag(EMovementMode MovementMode, uint8 CustomMovementMode, bool bTagEnabled)
{
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		const FGameplayTag* MovementModeTag = nullptr;
		if (MovementMode == MOVE_Custom)
		{
			MovementModeTag = ActGameplayTags::CustomMovementModeTagMap.Find(CustomMovementMode);
		}
		else
		{
			MovementModeTag = ActGameplayTags::MovementModeTagMap.Find(MovementMode);
		}

		if (MovementModeTag && MovementModeTag->IsValid())
		{
			ActASC->SetLooseGameplayTagCount(*MovementModeTag, (bTagEnabled ? 1 : 0));
		}
	}
}

void AActCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		ActASC->SetLooseGameplayTagCount(ActGameplayTags::Status_Crouching, 1);
	}

	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void AActCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		ActASC->SetLooseGameplayTagCount(ActGameplayTags::Status_Crouching, 0);
	}

	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

bool AActCharacter::CanJumpInternal_Implementation() const
{
	// same as ACharacter's implementation but without the crouch check
	return JumpIsAllowedInternal();
}

FSharedRepMovement::FSharedRepMovement()
{
	RepMovement.LocationQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
}

bool FSharedRepMovement::FillForCharacter(ACharacter* Character)
{
	if (USceneComponent* PawnRootComponent = Character->GetRootComponent())
	{
		UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement();

		RepMovement.Location = FRepMovement::RebaseOntoZeroOrigin(PawnRootComponent->GetComponentLocation(), Character);
		RepMovement.Rotation = PawnRootComponent->GetComponentRotation();
		RepMovement.LinearVelocity = CharacterMovement->Velocity;
		RepMovementMode = CharacterMovement->PackNetworkMovementMode();
		bProxyIsJumpForceApplied = Character->GetProxyIsJumpForceApplied() || (Character->JumpForceTimeRemaining > 0.0f);
		bIsCrouched = Character->IsCrouched();

		// Timestamp is sent as zero if unused
		if ((CharacterMovement->NetworkSmoothingMode == ENetworkSmoothingMode::Linear) || CharacterMovement->bNetworkAlwaysReplicateTransformUpdateTimestamp)
		{
			RepTimeStamp = CharacterMovement->GetServerLastTransformUpdateTimeStamp();
		}
		else
		{
			RepTimeStamp = 0.f;
		}

		return true;
	}
	return false;
}

bool FSharedRepMovement::Equals(const FSharedRepMovement& Other, ACharacter* Character) const
{
	if (RepMovement.Location != Other.RepMovement.Location)
	{
		return false;
	}

	if (RepMovement.Rotation != Other.RepMovement.Rotation)
	{
		return false;
	}

	if (RepMovement.LinearVelocity != Other.RepMovement.LinearVelocity)
	{
		return false;
	}

	if (RepMovementMode != Other.RepMovementMode)
	{
		return false;
	}

	if (bProxyIsJumpForceApplied != Other.bProxyIsJumpForceApplied)
	{
		return false;
	}

	if (bIsCrouched != Other.bIsCrouched)
	{
		return false;
	}

	return true;
}

bool FSharedRepMovement::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;
	RepMovement.NetSerialize(Ar, Map, bOutSuccess);
	Ar << RepMovementMode;
	Ar << bProxyIsJumpForceApplied;
	Ar << bIsCrouched;

	// Timestamp, if non-zero.
	uint8 bHasTimeStamp = (RepTimeStamp != 0.f);
	Ar.SerializeBits(&bHasTimeStamp, 1);
	if (bHasTimeStamp)
	{
		Ar << RepTimeStamp;
	}
	else
	{
		RepTimeStamp = 0.f;
	}

	return true;
}
