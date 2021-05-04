/*
 * Copyright (c) 2019, Technische Universität Kaiserslautern
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Lukas Steiner
 */

#include "SchedulerFrFcfs.h"
#include "../../configuration/Configuration.h"
#include "BufferCounterBankwise.h"
#include "BufferCounterReadWrite.h"
#include "BufferCounterShared.h"

using namespace tlm;

SchedulerFrFcfs::SchedulerFrFcfs()
{
    Configuration &config = Configuration::getInstance();
    buffer = std::vector<std::list<tlm_generic_payload *>>(config.memSpec->numberOfBanks);

    if (config.schedulerBuffer == Configuration::SchedulerBuffer::Bankwise)
        bufferCounter = new BufferCounterBankwise(config.requestBufferSize, config.memSpec->numberOfBanks);
    else if (config.schedulerBuffer == Configuration::SchedulerBuffer::ReadWrite)
        bufferCounter = new BufferCounterReadWrite(config.requestBufferSize);
    else if (config.schedulerBuffer == Configuration::SchedulerBuffer::Shared)
        bufferCounter = new BufferCounterShared(config.requestBufferSize);
}

SchedulerFrFcfs::~SchedulerFrFcfs()
{
    delete bufferCounter;
}

bool SchedulerFrFcfs::hasBufferSpace() const
{
    return bufferCounter->hasBufferSpace();
}

void SchedulerFrFcfs::storeRequest(tlm_generic_payload *payload)
{
    buffer[DramExtension::getBank(payload).ID()].push_back(payload);
    bufferCounter->storeRequest(payload);
}

void SchedulerFrFcfs::removeRequest(tlm_generic_payload *payload)
{
    bufferCounter->removeRequest(payload);
    unsigned bankID = DramExtension::getBank(payload).ID();
    for (auto it = buffer[bankID].begin(); it != buffer[bankID].end(); it++)
    {
        if (*it == payload)
        {
            buffer[bankID].erase(it);
            break;
        }
    }
}

tlm_generic_payload *SchedulerFrFcfs::getNextRequest(BankMachine *bankMachine) const
{
    unsigned bankID = bankMachine->getBank().ID();
    if (!buffer[bankID].empty())
    {
        if (bankMachine->getState() == BankMachine::State::Activated)
        {
            // Search for row hit
            Row openRow = bankMachine->getOpenRow();
            for (auto it = buffer[bankID].begin(); it != buffer[bankID].end(); it++)
            {
                if (DramExtension::getRow(*it) == openRow)
                    return *it;
            }
        }
        // No row hit found or bank precharged
        return buffer[bankID].front();
    }
    return nullptr;
}

bool SchedulerFrFcfs::hasFurtherRowHit(Bank bank, Row row) const
{
    unsigned rowHitCounter = 0;
    for (auto it = buffer[bank.ID()].begin(); it != buffer[bank.ID()].end(); it++)
    {
        if (DramExtension::getRow(*it) == row)
        {
            rowHitCounter++;
            if (rowHitCounter == 2)
                return true;
        }
    }
    return false;
}

bool SchedulerFrFcfs::hasFurtherRequest(Bank bank) const
{
    return (buffer[bank.ID()].size() >= 2);
}

const std::vector<unsigned> &SchedulerFrFcfs::getBufferDepth() const
{
    return bufferCounter->getBufferDepth();
}
