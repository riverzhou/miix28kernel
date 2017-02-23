/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20161117-64
 * Copyright (c) 2000 - 2016 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of ssdt4.aml, Wed Feb 22 22:30:40 2017
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x000000FF (255)
 *     Revision         0x01
 *     Checksum         0x26
 *     OEM ID           "INSYDE"
 *     OEM Table ID     "SoCDptf"
 *     OEM Revision     0x00000005 (5)
 *     Compiler ID      "INSY"
 *     Compiler Version 0x0100000D (16777229)
 */
DefinitionBlock ("", "SSDT", 1, "INSYDE", "SoCDptf", 0x00000005)
{
    External (_SB_.DPTF.CTOK, MethodObj)    // 1 Arguments
    External (_SB_.TCPU, UnknownObj)
    External (ACTT, UnknownObj)
    External (CRTT, UnknownObj)

    Scope (\_SB)
    {
        Device (TSOC)
        {
            Name (_HID, EisaId ("INT3402"))  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (CTYP, Zero)
            Name (CINT, 0x04)
            Name (LSTM, Zero)
            Name (PATC, 0x02)
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (Zero)
            }

            Method (_CRS, 0, Serialized)  // _CRS: Current Resource Settings
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive, ,, )
                    {
                        0x0000004A,
                    }
                })
                Return (RBUF) /* \_SB_.TSOC._CRS.RBUF */
            }

            Method (_CRT, 0, Serialized)  // _CRT: Critical Temperature
            {
                Return (\_SB.DPTF.CTOK (CRTT))
            }

            Method (_HOT, 0, Serialized)  // _HOT: Hot Temperature
            {
                Return (\_SB.DPTF.CTOK ((CRTT - 0x03)))
            }

            Method (_PSV, 0, Serialized)  // _PSV: Passive Temperature
            {
                Return (\_SB.DPTF.CTOK (ACTT))
            }

            Method (_SCP, 3, Serialized)  // _SCP: Set Cooling Policy
            {
                If (((Arg0 == Zero) || (Arg0 == One)))
                {
                    CTYP = Arg0
                    Notify (\_SB.TCPU, 0x91) // Device-Specific
                }
            }
        }
    }
}

