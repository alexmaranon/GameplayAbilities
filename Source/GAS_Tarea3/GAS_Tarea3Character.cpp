// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS_Tarea3Character.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"

#include "GASDataComponent.h"
#include "ASC.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AGAS_Tarea3Character

AGAS_Tarea3Character::AGAS_Tarea3Character()
{

	ASC = CreateDefaultSubobject<UASC>(TEXT("ASC"));

	CoreAttributeSet = CreateDefaultSubobject<UCoreAttributeSet>(TEXT("CoreAttr"));
	GASDataComponent = CreateDefaultSubobject<UGASDataComponent>(TEXT("GASData"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AGAS_Tarea3Character::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	SetAttributeCallbacks();
	if (ASC)
	{
		ASC->InitAbilityActorInfo(this, this);
	}
}



void AGAS_Tarea3Character::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer = GameplayStates;
}

void AGAS_Tarea3Character::AddTag(const FGameplayTag& InTag)
{
	GameplayStates.AddTag(InTag);
}

void AGAS_Tarea3Character::RemoveTag(const FGameplayTag& InTag)
{
	GameplayStates.RemoveTag(InTag);
}


void AGAS_Tarea3Character::ApplyGameplayEffect()
{
	if (ASC)
	{
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(SampleEffect, 1, EffectContext);

		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

UAbilitySystemComponent* AGAS_Tarea3Character::GetAbilitySystemComponent() const
{
	return ASC;
}

void AGAS_Tarea3Character::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

void AGAS_Tarea3Character::SetAttributeCallbacks()
{
	for (auto [Attribute, EffectorClass] : GASDataComponent->AttributeEffectors)
	{
		auto& Delegate = ASC->GetGameplayAttributeValueChangeDelegate(Attribute);

		UGameplayAttributeEffector* Effector = EffectorClass->GetDefaultObject<UGameplayAttributeEffector>();

		Delegate.AddUObject(Effector, &UGameplayAttributeEffector::ApplyAttributeEffector);
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AGAS_Tarea3Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		//EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		//EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGAS_Tarea3Character::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGAS_Tarea3Character::Look);

		//Abilities

		if (GASDataComponent)
		{
			for (auto [InputAction, AbilityClass] : GASDataComponent->InputAbilityMapping->Mappings)
			{
				if (InputAction)
				{
					if (AbilityClass)
					{
						if (ASC)
						{
							ASC->AddAbilityFromClass(AbilityClass);
							EnhancedInputComponent->BindAction(InputAction, ETriggerEvent::Started, this, &ThisClass::ExecuteAbility);
						}

					}
				}

			}
		}


	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AGAS_Tarea3Character::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr && !IsAttacking)
	{

		// Obtener la rotación de la cámara (sin Pitch ni Roll)
		FRotator CameraRotation = FollowCamera->GetComponentRotation();
		CameraRotation.Pitch = 0.0f;
		CameraRotation.Roll = 0.0f;

		// Obtener vectores de dirección a partir de la cámara
		FVector ForwardDirection = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::X);
		FVector RightDirection = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);

		// Calcular la dirección final del movimiento
		FVector MoveDirection = (ForwardDirection * MovementVector.Y) + (RightDirection * MovementVector.X);
		MoveDirection.Z = 0.0f; // Evitar movimientos en el eje Z

		// Rotar el personaje hacia la dirección de movimiento
		if (!MoveDirection.IsNearlyZero())
		{
			FRotator TargetRotation = MoveDirection.Rotation();
			TargetRotation.Pitch = 0.0f;
			TargetRotation.Roll = 0.0f;
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, GetWorld()->GetDeltaSeconds(), 10.0f));
		}

		// Aplicar el movimiento
		AddMovementInput(MoveDirection.GetSafeNormal());

	}
}

void AGAS_Tarea3Character::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}


void AGAS_Tarea3Character::ExecuteAbility(const FInputActionInstance& InputActionInstance)
{

	if (GASDataComponent)
	{
		if (ASC)
		{
			const UInputAction* Action = InputActionInstance.GetSourceAction();
			if (Action)
			{

				ASC->TryActivateAbilityByClass(GASDataComponent->InputAbilityMapping->Mappings[Action]);
			}

		}
	}
}