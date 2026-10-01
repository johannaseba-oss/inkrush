#include "ScoreKeeper.h"
#include "ShadowCat.h"
#include "Kismet/GameplayStatics.h"

const TCHAR* UScoreKeeper::SlotName = TEXT("ShadowCat");

void UScoreKeeper::Load()
{
	Data = Cast<UCatRunSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Data)
	{
		Data = Cast<UCatRunSaveGame>(UGameplayStatics::CreateSaveGameObject(UCatRunSaveGame::StaticClass()));
		UE_LOG(LogShadowCat, Log, TEXT("Neuer Speicherstand"));
	}
	else
	{
		UE_LOG(LogShadowCat, Log, TEXT("Speicherstand geladen: Highscore %d"), Data->HighScore);
	}
}

void UScoreKeeper::Save()
{
	if (bReadOnly)
	{
		return;
	}
	if (Data && !UGameplayStatics::SaveGameToSlot(Data, SlotName, 0))
	{
		UE_LOG(LogShadowCat, Warning, TEXT("Speichern fehlgeschlagen"));
	}
}

void UScoreKeeper::BeginRun()
{
	Meters = 0.f;
	Bonus = 0;
}

void UScoreKeeper::SetMeters(float InMeters)
{
	Meters = FMath::Max(Meters, InMeters);
}

bool UScoreKeeper::CommitTime(float Seconds)
{
	if (!Data)
	{
		Load();
	}
	const bool bBest = Data->BestTime <= 0.f || Seconds < Data->BestTime;
	if (bBest)
	{
		Data->BestTime = Seconds;
	}
	Save();
	return bBest;
}

bool UScoreKeeper::CommitRun()
{
	if (!Data)
	{
		Load();
	}
	const int32 Score = GetScore();
	const bool bRecord = Score > Data->HighScore;
	if (bRecord)
	{
		Data->HighScore = Score;
	}
	Data->BestMeters = FMath::Max(Data->BestMeters, FMath::FloorToInt(Meters));
	Save();
	UE_LOG(LogShadowCat, Log, TEXT("Lauf beendet: Score %d (%.0f m + %d Bonus), Highscore %d%s"), Score, Meters, Bonus, Data->HighScore, bRecord ? TEXT(" NEU") : TEXT(""));
	return bRecord;
}
