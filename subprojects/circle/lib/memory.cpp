//
// memory.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2016  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <circle/memory.h>
#include <circle/armv6mmu.h>
#include <circle/bcmpropertytags.h>
#include <circle/alloc.h>
#include <circle/synchronize.h>
#include <circle/spinlock.h>
#include <circle/sysconfig.h>
#include <assert.h>

#define MMU_MODE	(  ARM_CONTROL_MMU			\
			 | ARM_CONTROL_L1_CACHE			\
			 | ARM_CONTROL_L1_INSTRUCTION_CACHE	\
			 | ARM_CONTROL_BRANCH_PREDICTION)

#define TTBCR_SPLIT	2

CMemorySystem::CMemorySystem (boolean bEnableMMU)
#ifndef USE_RPI_STUB_AT
:	m_bEnableMMU (bEnableMMU),
	m_nMemSize (0),
#else
:	m_bEnableMMU (FALSE),
	m_nMemSize (USE_RPI_STUB_AT),
#endif
	m_pPageTable0Default (0),
	m_pPageTable1 (0)
{
#ifndef USE_RPI_STUB_AT
	CBcmPropertyTags Tags;
	TPropertyTagMemory TagMemory;
	if (!Tags.GetTag (PROPTAG_GET_ARM_MEMORY, &TagMemory, sizeof TagMemory))
	{
		TagMemory.nBaseAddress = 0;
		TagMemory.nSize = ARM_MEM_SIZE;
	}

	assert (TagMemory.nBaseAddress == 0);
	m_nMemSize = TagMemory.nSize;

	mem_init (TagMemory.nBaseAddress, m_nMemSize);
#else
	mem_init (0, m_nMemSize);
#endif

	if (m_bEnableMMU)
	{
		m_pPageTable0Default = new CPageTable (m_nMemSize);
		assert (m_pPageTable0Default != 0);

		m_pPageTable1 = new CPageTable;
		assert (m_pPageTable1 != 0);

		EnableMMU ();
	}
}

CMemorySystem::~CMemorySystem (void)
{
	if (m_bEnableMMU)
	{
		// disable MMU
		u32 nControl;
		asm volatile ("mrc p15, 0, %0, c1, c0,  0" : "=r" (nControl));
		nControl &=  ~MMU_MODE;
		asm volatile ("mcr p15, 0, %0, c1, c0,  0" : : "r" (nControl) : "memory");

		// invalidate unified TLB (if MMU is re-enabled later)
		asm volatile ("mcr p15, 0, %0, c8, c7,  0" : : "r" (0) : "memory");
	}
	
	delete m_pPageTable1;
	m_pPageTable1 = 0;
	
	delete m_pPageTable0Default;
	m_pPageTable0Default = 0;
}

u32 CMemorySystem::GetMemSize (void) const
{
	return m_nMemSize;
}

void CMemorySystem::EnableMMU (void)
{
	assert (m_bEnableMMU);

	u32 nAuxControl;
	asm volatile ("mrc p15, 0, %0, c1, c0,  1" : "=r" (nAuxControl));

    nAuxControl |= ARM_AUX_CONTROL_SMP;

	asm volatile ("mcr p15, 0, %0, c1, c0,  1" : : "r" (nAuxControl));

	u32 nTLBType;
	asm volatile ("mrc p15, 0, %0, c0, c0,  3" : "=r" (nTLBType));
	assert (!(nTLBType & ARM_TLB_TYPE_SEPARATE_TLBS));

	// set TTB control
	asm volatile ("mcr p15, 0, %0, c2, c0,  2" : : "r" (TTBCR_SPLIT));

	// set TTBR0
	assert (m_pPageTable0Default != 0);
	asm volatile ("mcr p15, 0, %0, c2, c0,  0" : : "r" (m_pPageTable0Default->GetBaseAddress ()));

	// set TTBR1
	assert (m_pPageTable1 != 0);
	asm volatile ("mcr p15, 0, %0, c2, c0,  1" : : "r" (m_pPageTable1->GetBaseAddress ()));
	
	// set Domain Access Control register (Domain 0 and 1 to client)
	asm volatile ("mcr p15, 0, %0, c3, c0,  0" : : "r" (  DOMAIN_CLIENT << 0
							    | DOMAIN_CLIENT << 2));

    InvalidateDataCache ();

	// required if MMU was previously enabled and not properly reset
	InvalidateInstructionCache ();
	FlushBranchTargetCache ();
	asm volatile ("mcr p15, 0, %0, c8, c7,  0" : : "r" (0));	// invalidate unified TLB
	DataSyncBarrier ();
	FlushPrefetchBuffer ();

	// enable MMU
	u32 nControl;
    asm volatile ("mrc p15, 0, %0, c1, c0,  0" : "=r" (nControl));
	nControl |= MMU_MODE;
	asm volatile ("mcr p15, 0, %0, c1, c0,  0" : : "r" (nControl) : "memory");
}
