#pragma once

#include "../../Flare.h"
#include "../Components/FlareButton.h"
#include "../Components/FlareDropList.h"


struct FFlareCompanyDescription;
class AFlareMenuManager;


class SFlareSkirmishSetupMenu : public SCompoundWidget
{
	/*----------------------------------------------------
		Slate arguments
	----------------------------------------------------*/

	SLATE_BEGIN_ARGS(SFlareSkirmishSetupMenu){}

	SLATE_ARGUMENT(TWeakObjectPtr<class AFlareMenuManager>, MenuManager)
	
	SLATE_END_ARGS()


public:

	/*----------------------------------------------------
		Interaction
	----------------------------------------------------*/

	/** Create the widget */
	void Construct(const FArguments& InArgs);

	/** Setup the widget */
	void Setup();

	/** Enter this menu */
	void Enter();

	/** Exit this menu */
	void Exit();


protected:

	/*----------------------------------------------------
		Content callbacks
	----------------------------------------------------*/

	TSharedRef<SWidget> OnGenerateCompanyComboLine(FFlareCompanyDescription Item);
	void OnCompanyComboLineSelectionChanged(FFlareCompanyDescription Item, ESelectInfo::Type SelectInfo);
	FText OnGetCurrentCompanyComboLine() const;


	/*----------------------------------------------------
		Callbacks
	----------------------------------------------------*/

	/** Start the process of ordering a new ship */
	void OnOrderShip(bool ForPlayer);

	/** Order a new ship */
	void OnOrderShipConfirmed(FFlareSpacecraftDescription* Spacecraft);

	/** Start engagement */
	void OnStartSkirmish();

	/** Quit the menu */
	void OnMainMenu();


protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	bool                                        IsOrderingForPlayer;

	TWeakObjectPtr<class AFlareMenuManager>     MenuManager;

	TSharedPtr<SFlareDropList<FFlareCompanyDescription>> CompanySelector;

};