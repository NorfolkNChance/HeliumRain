
#include "Flare.h"
#include "../Game/FlareGame.h"
#include "../Data/FlareQuestCatalog.h"
#include "../Data/FlareQuestCatalogEntry.h"
#include "../Player/FlarePlayerController.h"
#include "FlareQuestGenerator.h"
#include "FlareQuestManager.h"
#include "FlareCatalogQuest.h"
#include "QuestCatalog/FlareTutorialQuest.h"

#define LOCTEXT_NAMESPACE "FlareQuestManager"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareQuestManager::UFlareQuestManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


/*----------------------------------------------------
	Save
----------------------------------------------------*/

void UFlareQuestManager::Load(const FFlareQuestSave& Data)
{
	Game = Cast<AFlareGame>(GetOuter());

	// Init the quest manager
	QuestGenerator = NewObject<UFlareQuestGenerator>(this, UFlareQuestGenerator::StaticClass());
	QuestGenerator->Load(this, Data);

	QuestData = Data;

	ActiveQuestIdentifiers.Empty();
	for (int QuestProgressIndex = 0; QuestProgressIndex <Data.QuestProgresses.Num(); QuestProgressIndex++)
	{
		ActiveQuestIdentifiers.Add(Data.QuestProgresses[QuestProgressIndex].QuestIdentifier);
	}

	LoadBuildinQuest();

	LoadCatalogQuests();

	LoadDynamicQuests();


	for(UFlareQuest* Quest: Quests)
	{
		LoadCallbacks(Quest);
		Quest->UpdateState();
	}

	if (!SelectedQuest)
	{
		AutoSelectQuest();
	}
}

void UFlareQuestManager::LoadBuildinQuest()
{
	AddQuest(UFlareQuestTutorialFlying::Create(this));
	AddQuest(UFlareQuestTutorialNavigation::Create(this));
}

void UFlareQuestManager::LoadCatalogQuests()
{
	for (int QuestIndex = 0; QuestIndex <Game->GetQuestCatalog()->Quests.Num(); QuestIndex++)
	{
		FFlareQuestDescription* QuestDescription = &(Game->GetQuestCatalog()->Quests[QuestIndex]->Data);

		// Create the quest
		UFlareCatalogQuest* Quest = NewObject<UFlareCatalogQuest>(this, UFlareCatalogQuest::StaticClass());
		Quest->Load(this, QuestDescription);

		AddQuest(Quest);
	}
}

void UFlareQuestManager::LoadDynamicQuests()
{
	QuestGenerator->LoadQuests(QuestData);
}

void UFlareQuestManager::AddQuest(UFlareQuest* Quest)
{
	int QuestProgressIndex = ActiveQuestIdentifiers.IndexOfByKey(Quest->GetIdentifier());

	// Setup quest index
	int32 StepIndex = 0;
	for(UFlareQuestStep* Step : Quest->GetSteps())
	{
		Step->SetStepIndex(StepIndex++);
	}

	// Skip tutorial quests.
	if (Quest->GetQuestCategory() == EFlareQuestCategory::TUTORIAL && !QuestData.PlayTutorial)
	{
		FLOGV("Found skipped tutorial quest %s", *Quest->GetIdentifier().ToString());
		OldQuests.Add(Quest);
		Quest->SetStatus(EFlareQuestStatus::SUCCESSFUL);
	}
	else if (QuestProgressIndex != INDEX_NONE)
	{
		FLOGV("Found active quest %s", *Quest->GetIdentifier().ToString());

		// Current quests
		ActiveQuests.Add(Quest);
		Quest->Restore(QuestData.QuestProgresses[QuestProgressIndex]);
		if (QuestData.SelectedQuest == Quest->GetIdentifier())
		{
			SelectQuest(Quest);
		}
	}
	else if (QuestData.SuccessfulQuests.Contains(Quest->GetIdentifier()))
	{
		FLOGV("Found completed quest %s", *Quest->GetIdentifier().ToString());
		OldQuests.Add(Quest);
		Quest->SetStatus(EFlareQuestStatus::SUCCESSFUL);
	}
	else if (QuestData.AbandonedQuests.Contains(Quest->GetIdentifier()))
	{
		FLOGV("Found abandoned quest %s", *Quest->GetIdentifier().ToString());
		OldQuests.Add(Quest);
		Quest->SetStatus(EFlareQuestStatus::ABANDONED);
	}
	else if (QuestData.FailedQuests.Contains(Quest->GetIdentifier()))
	{
		FLOGV("Found failed quest %s", *Quest->GetIdentifier().ToString());
		OldQuests.Add(Quest);
		Quest->SetStatus(EFlareQuestStatus::FAILED);
	}
	else if (QuestData.AvailableQuests.Contains(Quest->GetIdentifier()))
	{
		FLOGV("Found available quest %s", *Quest->GetIdentifier().ToString());
		AvailableQuests.Add(Quest);
		Quest->SetStatus(EFlareQuestStatus::AVAILABLE);
	}
	else
	{
		FLOGV("Found pending quest %s", *Quest->GetIdentifier().ToString());
		PendingQuests.Add(Quest);
		Quest->SetStatus(EFlareQuestStatus::PENDING);
	}

	Quests.Add(Quest);
}


FFlareQuestSave* UFlareQuestManager::Save()
{
	QuestData.QuestProgresses.Empty();
	QuestData.SuccessfulQuests.Empty();
	QuestData.AbandonedQuests.Empty();
	QuestData.FailedQuests.Empty();

	QuestData.SelectedQuest = (SelectedQuest ? SelectedQuest->GetIdentifier() : NAME_None);

	for (int QuestIndex = 0; QuestIndex < ActiveQuests.Num(); QuestIndex++)
	{
		UFlareQuest* Quest = ActiveQuests[QuestIndex];
		FFlareQuestProgressSave* QuestProgressSave = Quest->Save();
		QuestData.QuestProgresses.Add(*QuestProgressSave);
	}

	for (int QuestIndex = 0; QuestIndex < OldQuests.Num(); QuestIndex++)
	{
		UFlareQuest* Quest = OldQuests[QuestIndex];
		switch (Quest->GetStatus())
		{
			case EFlareQuestStatus::SUCCESSFUL:
				QuestData.SuccessfulQuests.Add(Quest->GetIdentifier());
				break;
			case EFlareQuestStatus::ABANDONED:
				QuestData.AbandonedQuests.Add(Quest->GetIdentifier());
				break;
			case EFlareQuestStatus::FAILED:
				QuestData.FailedQuests.Add(Quest->GetIdentifier());
				break;
			default:
				FLOGV("Bad status %d for quest %s", (int) (Quest->GetStatus() + 0), *Quest->GetIdentifier().ToString());
		}
	}

	QuestGenerator->Save(QuestData);

	return &QuestData;
}


/*----------------------------------------------------
	Quest management
----------------------------------------------------*/

void UFlareQuestManager::AcceptQuest(UFlareQuest* Quest)
{
	FLOGV("Accept quest %s", *Quest->GetIdentifier().ToString());

	Quest->Accept();
}

void UFlareQuestManager::AbandonQuest(UFlareQuest* Quest)
{
	FLOGV("Abandon quest %s", *Quest->GetIdentifier().ToString());

	Quest->Abandon();
}

void UFlareQuestManager::SelectQuest(UFlareQuest* Quest)
{
	FLOGV("Select quest %s", *Quest->GetIdentifier().ToString());
	if (!IsQuestActive(Quest))
	{
		FLOGV("ERROR: Fail to select quest %s. The quest to select must be active", *Quest->GetIdentifier().ToString());
		return;
	}

	if (SelectedQuest)
	{
		SelectedQuest->StopObjectiveTracking();
	}

	SelectedQuest = Quest;
	SelectedQuest->StartObjectiveTracking();
}

void UFlareQuestManager::AutoSelectQuest()
{
	if (ActiveQuests.Num()> 1)
	{
		SelectQuest(ActiveQuests[0]);
	}
	else
	{
		if (SelectedQuest)
		{
			SelectedQuest->StopObjectiveTracking();
		}
		SelectedQuest = NULL;
	}
}


/*----------------------------------------------------
	Callbacks
----------------------------------------------------*/

void UFlareQuestManager::LoadCallbacks(UFlareQuest* Quest)
{
	ClearCallbacks(Quest);

	TArray<EFlareQuestCallback::Type> Callbacks = Quest->GetCurrentCallbacks();

	for (int i = 0; i < Callbacks.Num(); i++)
	{
		switch (Callbacks[i])
		{
		case EFlareQuestCallback::FLY_SHIP:
			FlyShipCallback.Add(Quest);
			break;
		case EFlareQuestCallback::TICK_FLYING:
			TickFlyingCallback.Add(Quest);
			break;
		case EFlareQuestCallback::SECTOR_VISITED:
			SectorVisitedCallback.Add(Quest);
			break;
		case EFlareQuestCallback::SECTOR_ACTIVE:
			SectorActiveCallback.Add(Quest);
			break;
		case EFlareQuestCallback::QUEST:
			QuestCallback.Add(Quest);
			break;
		default:
			FLOGV("Bad callback type %d for quest %s", (int)(Callbacks[i] + 0), *Quest->GetIdentifier().ToString());
		}
	}
}

void UFlareQuestManager::ClearCallbacks(UFlareQuest* Quest)
{
	TickFlyingCallback.Remove(Quest);
	FlyShipCallback.Remove(Quest);
}

void UFlareQuestManager::OnTick(float DeltaSeconds)
{
	if (GetGame()->GetActiveSector())
	{
		// Tick TickFlying callback only if there is an active sector
		for (int i = 0; i < TickFlyingCallback.Num(); i++)
		{
			TickFlyingCallback[i]->OnTick(DeltaSeconds);
		}
	}
}

void UFlareQuestManager::OnFlyShip(AFlareSpacecraft* Ship)
{
	for (int i = 0; i < FlyShipCallback.Num(); i++)
	{
		FlyShipCallback[i]->OnFlyShip(Ship);
	}
}

void UFlareQuestManager::OnSectorActivation(UFlareSimulatedSector* Sector)
{
	for (int i = 0; i < SectorActiveCallback.Num(); i++)
	{
		SectorActiveCallback[i]->OnSectorActivation(Sector);
	}
}

void UFlareQuestManager::OnSectorVisited(UFlareSimulatedSector* Sector)
{
	for (int i = 0; i < SectorVisitedCallback.Num(); i++)
	{
		SectorVisitedCallback[i]->OnSectorVisited(Sector);
	}
}


void UFlareQuestManager::OnTravelEnded(UFlareFleet* Fleet)
{
	if (Fleet == Game->GetPC()->GetPlayerFleet())
	{
		// Player end travel, try to generate a quest in the destination sector
		QuestGenerator->GenerateSectorQuest(Fleet->GetCurrentSector());
	}
}

void UFlareQuestManager::OnQuestStatusChanged(UFlareQuest* Quest)
{
	LoadCallbacks(Quest);

	for (int i = 0; i < QuestCallback.Num(); i++)
	{
		QuestCallback[i]->OnQuestStatusChanged(Quest);
	}
}

void UFlareQuestManager::OnQuestSuccess(UFlareQuest* Quest)
{
	FLOGV("Quest %s is now successful", *Quest->GetIdentifier().ToString())
		ActiveQuests.Remove(Quest);
	OldQuests.Add(Quest);

	// Quest successful notification
	if (Quest->GetQuestCategory() != EFlareQuestCategory::TUTORIAL)
	{
		FText Text = LOCTEXT("Quest successful", "Contract successful");
		FText Info = Quest->GetQuestName();

		FFlareMenuParameterData Data;
		Data.Quest = Quest;

		GetGame()->GetPC()->Notify(Text, Info, FName(*(FString("quest-") + Quest->GetIdentifier().ToString() + "-status")),
			EFlareNotification::NT_Quest, false, EFlareMenu::MENU_Quest, Data);
	}

	if (Quest == SelectedQuest)
	{
		AutoSelectQuest();
	}

	OnQuestStatusChanged(Quest);

}

void UFlareQuestManager::OnQuestFail(UFlareQuest* Quest)
{
	FLOGV("Quest %s is now failed", *Quest->GetIdentifier().ToString())
		ActiveQuests.Remove(Quest);
	OldQuests.Add(Quest);

	// Quest failed notification
	if (Quest->GetQuestCategory() != EFlareQuestCategory::TUTORIAL)
	{
		FText Text = LOCTEXT("Quest failed", "Contract failed");
		FText Info = Quest->GetQuestName();

		FFlareMenuParameterData Data;
		Data.Quest = Quest;

		GetGame()->GetPC()->Notify(Text, Info, FName(*(FString("quest-") + Quest->GetIdentifier().ToString() + "-status")),
			EFlareNotification::NT_Quest, false, EFlareMenu::MENU_Quest, Data);
	}

	if (Quest == SelectedQuest)
	{
		AutoSelectQuest();
	}
	OnQuestStatusChanged(Quest);
}

void UFlareQuestManager::OnQuestAvailable(UFlareQuest* Quest)
{
	FLOGV("Quest %s is now available", *Quest->GetIdentifier().ToString())
	PendingQuests.Remove(Quest);
	AvailableQuests.Add(Quest);

	// New quest notification
	if (Quest->GetQuestCategory() != EFlareQuestCategory::TUTORIAL)
	{
		FText Text = LOCTEXT("New quest", "New contract available");
		FText Info = Quest->GetQuestName();

		FFlareMenuParameterData Data;
		Data.Quest = Quest;

		GetGame()->GetPC()->Notify(Text, Info, FName(*(FString("quest-") + Quest->GetIdentifier().ToString() + "-status")),
			EFlareNotification::NT_Quest, false, EFlareMenu::MENU_Quest, Data);
	}

	OnQuestStatusChanged(Quest);
}

void UFlareQuestManager::OnQuestActivation(UFlareQuest* Quest)
{
	FLOGV("Quest %s is now active", *Quest->GetIdentifier().ToString())
	AvailableQuests.Remove(Quest);
	ActiveQuests.Add(Quest);

	// New quest notification
	if (Quest->GetQuestCategory() != EFlareQuestCategory::TUTORIAL)
	{
		FText Text = LOCTEXT("New quest", "New contract started");
		FText Info = Quest->GetQuestName();

		FFlareMenuParameterData Data;
		Data.Quest = Quest;

		GetGame()->GetPC()->Notify(Text, Info, FName(*(FString("quest-") + Quest->GetIdentifier().ToString() + "-status")),
			EFlareNotification::NT_Quest, false, EFlareMenu::MENU_Quest, Data);
	}

	if (!SelectedQuest)
	{
		SelectQuest(Quest);
	}
	OnQuestStatusChanged(Quest);
}


/*----------------------------------------------------
	Getters
----------------------------------------------------*/

bool UFlareQuestManager::IsQuestActive(UFlareQuest* Quest)
{
	for(UFlareQuest* ActiveQuest: ActiveQuests)
	{
		if (Quest == ActiveQuest)
		{
			return true;
		}
	}
	return false;
}

bool UFlareQuestManager::IsOldQuest(UFlareQuest* Quest)
{
	return IsQuestSuccessfull(Quest) || IsQuestFailed(Quest);
}

bool UFlareQuestManager::IsQuestAvailable(UFlareQuest* Quest)
{
	for (UFlareQuest* AvailableQuest : AvailableQuests)
	{
		if (AvailableQuest == Quest)
		{
			return true;
		}
	}
	return false;
}

bool UFlareQuestManager::IsQuestSuccessfull(UFlareQuest* Quest)
{
	for(UFlareQuest* OldQuest: OldQuests)
	{
		if (OldQuest == Quest)
		{
			if (Quest->GetStatus() == EFlareQuestStatus::SUCCESSFUL)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

bool UFlareQuestManager::IsQuestFailed(UFlareQuest* Quest)
{
	for(UFlareQuest* OldQuest: OldQuests)
	{
		if (OldQuest == Quest)
		{
			if (Quest->GetStatus() == EFlareQuestStatus::FAILED)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

UFlareQuest* UFlareQuestManager::FindQuest(FName QuestIdentifier)
{
	for(UFlareQuest* Quest: ActiveQuests)
	{
		if (Quest->GetIdentifier() == QuestIdentifier)
		{
			return Quest;
		}
	}

	for(UFlareQuest* Quest: OldQuests)
	{
		if (Quest->GetIdentifier() == QuestIdentifier)
		{
			return Quest;
		}
	}

	for(UFlareQuest* Quest: AvailableQuests)
	{
		if (Quest->GetIdentifier() == QuestIdentifier)
		{
			return Quest;
		}
	}

	return NULL;
}


#undef LOCTEXT_NAMESPACE
