// SARTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <cstdint>

typedef struct
{
    uint8_t Type;
    uint8_t ByteCount;
} COLINFO;

typedef struct {
    int NumCols;
    int NumRows;
    int BytesPerRow;
    COLINFO *MetaData;
    uint8_t *Data;
} SARTABLEINFO;

const int MaxSarTableCount = 8;
typedef struct {
    int TableCount;
    SARTABLEINFO Table[MaxSarTableCount];
} SARTABLE;

bool LoadTriggerTable(SARTABLE *TriggerTable);
bool LoadActionTable(SARTABLE *ActionTable);
int GetSarTableId();
int FindTrigger(SARTABLEINFO *TriggerTable);
bool ApplyAction(int Row, SARTABLEINFO *ActionTable);

int main(int argc, char *argv[])
{
    SARTABLE TriggerTable;
    SARTABLE ActionTable;

    LoadTriggerTable(&TriggerTable);
    LoadActionTable(&ActionTable);

    int SarTableId = GetSarTableId();

    int RowSelection;
    RowSelection = FindTrigger(&TriggerTable.Table[SarTableId]);
    ApplyAction(RowSelection, &ActionTable.Table[SarTableId]);

    return 0;
}

bool LoadTable(SARTABLEINFO *TableInfo, const uint8_t *TableData)
{
    // Read NumCols
    TableInfo->NumCols = (int)TableData[0];

    // Read NumRows
    TableInfo->NumRows = (int)TableData[1];

    // Get pointer to column metadata 
    TableInfo->MetaData = (COLINFO *)&TableData[2];

    // Get pointer to start of trigger data
    TableInfo->Data = (uint8_t *)&TableInfo->MetaData[TableInfo->NumCols];

    // Count up bytes per row to simplify moving to next row
    TableInfo->BytesPerRow = 0;
    for (int Col = 0; Col < TableInfo->NumCols; Col++) {
        TableInfo->BytesPerRow += TableInfo->MetaData[Col].ByteCount;
    }

    return true;
}

bool LoadSarTable(SARTABLE *SarTable, const uint8_t *TableData)
{
    SarTable->TableCount = (int)*(uint32_t *)TableData;
    uint32_t *Offset = (uint32_t *)(TableData + sizeof(uint32_t));
    uint8_t *DataStart = (uint8_t *)&Offset[SarTable->TableCount];
    for (int i = 0; i < SarTable->TableCount; i++) {
        LoadTable(&SarTable->Table[i], &DataStart[Offset[i]]);
    }

    return true;
}

bool GetMccFromLteModem(int &Mcc)
{
    Mcc = 0x360;
    return true;
}

bool GetTableIdFromMcc(int Mcc, int &TableId)
{
    TableId = 0;
    return true;
}

bool GetCountryCodeFromWiFi(char *CountryCode)
{
    memcpy(CountryCode, "DE", 2);
    return true;
}

bool GetTableIdFromCountryCode(char *CountryCode, int &TableId)
{
    TableId = 0;
    return true;
}

int GetSarTableId()
{
    int TableId;
    int Mcc;
    if (GetMccFromLteModem(Mcc) &&
        GetTableIdFromMcc(Mcc, TableId))
    {
        return TableId;
    }

    char CountryCode[2];
    if (GetCountryCodeFromWiFi(CountryCode) &&
        GetTableIdFromCountryCode(CountryCode, TableId))
    {
        return TableId;
    }

    printf("Could not determine SAR trigger table ID\n");
    return 0;
}


/* Triggers */

bool TestPes(uint8_t *Data)
{
    uint8_t TestPesData = 1;
    uint8_t PesData = *(uint8_t *)Data;
    return (PesData == TestPesData);
}

bool TestPosture(uint8_t *Data)
{
    uint8_t TestPostureData = 1;
    uint8_t PostureData = *(uint8_t *)Data;
    return (PostureData == TestPostureData);
}

typedef enum TriggerType
{
    Pes = 0,
    Posture,
    SimultaneousXmit
} TriggerType;

int FindTrigger(SARTABLEINFO *TriggerTable)
{
    // Iterate through table looking for a row where all values match.
    uint8_t *TriggerRowPtr = TriggerTable->Data;
    for (int Row = 0; Row < TriggerTable->NumRows; Row++) {
        bool match = true;
        uint8_t *TriggerColPtr = TriggerRowPtr;
        for (int Col = 0; Col < TriggerTable->NumCols; Col++) {
            switch (TriggerTable->MetaData[Col].Type) {
            case Pes:
                match = TestPes(TriggerColPtr);
                break;
            case Posture:
                match = TestPosture(TriggerColPtr);
                break;
            default:
                // Note: as an example, SimultaneousXmit is unrecognized/unimplemented
                printf("Unrecognized trigger type %d, skipping\n", TriggerTable->MetaData[Col].Type);
                break;
            }

            if (!match) {
                break;
            }

            TriggerColPtr += TriggerTable->MetaData[Col].ByteCount;
        }

        if (match) {
            printf("Found match at row %d\n", Row);
            return Row;
        }

        // Go to next row
        TriggerRowPtr += TriggerTable->BytesPerRow;
    }

    printf("FindTrigger: Critical error, no match found\n");
    return 0; // What to return if no match? Should never happen.
}

bool LoadTriggerTable(SARTABLE *TableInfo)
{
    static const uint8_t TriggerTableData[] =
    {
        4,0,0,0,    // 4 tables (little endian)
        0,0,0,0,    // table 0 offset
        0,0,0,0,    // table 1 offset
        0,0,0,0,    // table 2 offset
        0,0,0,0,    // table 3 offset

        // Table 0
        3, 4,       // NumCols, NumRows

        Pes, 1,                 // Assume Pes needs one byte of data (e.g. on/off)
        Posture, 1,             // Assume Posture needs one byte of data (e.g on/off)
        SimultaneousXmit, 1,    // Assume Simultaneous transmit needs one byte of data (e.g. on/off)

        // Pes      Posture     SimultaneousXmit
        0,          1,          1,
        0,          1,          0,
        1,          0,          0,
        1,          1,          1
    };

    return LoadSarTable(TableInfo, TriggerTableData);
}

/***********/
/* Actions */
/***********/

void SetWiFiPower(uint8_t *Data)
{
    const char *ChainAEnable = Data[0] ? "Enabled" : "Disabled";
    const char *ChainBEnable = Data[1] ? "Enabled" : "Disabled";
    printf("Set WiFi Power: Chain A %s (%d), Chain B %s (%d)\n", ChainAEnable, Data[2], ChainBEnable, Data[3]);
}

void SetLTEPower(uint8_t *Data)
{
    const char *SAREnable = Data[0] ? "Enabled" : "Disabled";
    printf("Set LTE Power: SAR %s (%d)\n", SAREnable, Data[1]);
}

typedef enum ActionType
{
    WiFiPower,
    LTEPower
} ActionType;

bool ApplyAction(int Row, SARTABLEINFO *ActionTable)
{
    uint8_t *TriggerColPtr = ActionTable->Data + (Row * ActionTable->BytesPerRow);
    for (int Col = 0; Col < ActionTable->NumCols; Col++) {
        switch (ActionTable->MetaData[Col].Type) {
        case WiFiPower:
            SetWiFiPower(TriggerColPtr);
            break;
        case LTEPower:
            SetLTEPower(TriggerColPtr);
            break;
        default:
            // Note: action unsupported
            printf("Unrecognized action type %d, skipping\n", ActionTable->MetaData[Col].Type);
            break;
        }
        TriggerColPtr += ActionTable->MetaData[Col].ByteCount;
    }

    return true;
}

bool LoadActionTable(SARTABLE *TableInfo)
{
    static const uint8_t ActionTableData[] =
    {
        4,0,0,0,    // 4 tables (little endian)
        0,0,0,0,    // table 0 offset
        0,0,0,0,    // table 1 offset
        0,0,0,0,    // table 2 offset
        0,0,0,0,    // table 3 offset

        // Table 0
        2, 4,       // NumCols, NumRows

        // Array of COLINFO structs, one per column, holding metadata about what types of actions are supported.
        WiFiPower, 4,   // WiFiPower needs 4 bytes: ChainASarEnable, ChainBSarEnable, ChainAPowerIndex, ChainBPowerIndex
        LTEPower, 2,    // LTEPower needs 2 bytes: SAREnable, SARPowerIndex

        // Data values:
        //  WiFi(4 bytes)     LTE(2 bytes)
        1, 1, 10, 20,         0, 10,
        0, 1, 30, 40,         1, 20,
        1, 0, 50, 60,         0, 30,
        1, 1, 70, 80,         1, 40
    };

    return LoadSarTable(TableInfo, ActionTableData);
}
