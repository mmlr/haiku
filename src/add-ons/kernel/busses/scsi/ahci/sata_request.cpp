/*
 * Copyright 2008, Marcus Overhagen. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <string.h>

#include "sata_request.h"
#include "scsi_cmds.h"


#define FIS_TYPE_REGISTER_HOST_TO_DEVICE 0x27


sata_request::sata_request()
	:
	fCcb(NULL),
	fIsATAPI(false),
	fCompletionSem(create_sem(0, "sata completion")),
	fCompletionStatus(0),
	fData(NULL),
	fDataSize(0)
{
}


sata_request::sata_request(scsi_ccb* ccb)
	:
	fCcb(ccb),
	fIsATAPI(false),
	fCompletionSem(-1),
	fCompletionStatus(0),
	fData(NULL),
	fDataSize(0)
{
}


sata_request::~sata_request()
{
	if (fCompletionSem >= 0)
		delete_sem(fCompletionSem);
}


void
sata_request::SetData(void* data, size_t dataSize)
{
	ASSERT(fCcb == NULL);
	fData = data;
	fDataSize = dataSize;
}


void
sata_request::SetATACommand(uint8 command)
{
	memset(fFis, 0, sizeof(fFis));
	fFis[0] = FIS_TYPE_REGISTER_HOST_TO_DEVICE;
	fFis[1] = 0x80;
		// This is a command
	fFis[2] = command;
}


void
sata_request::SetATA28Command(uint8 command, uint32 lba, uint8 sectorCount)
{
	SetATACommand(command);
	fFis[4] = lba & 0xff;
	fFis[5] = (lba >> 8) & 0xff;
	fFis[6] = (lba >> 16) & 0xff;
	fFis[7] = 0x40 | ((lba >> 24) & 0x0f);
		// device
	fFis[12] = sectorCount & 0xff;
}


void
sata_request::SetATA48Command(uint8 command, uint64 lba, uint16 sectorCount)
{
	SetATACommand(command);
	fFis[4] = lba & 0xff;
	fFis[5] = (lba >> 8) & 0xff;
	fFis[6] = (lba >> 16) & 0xff;
	fFis[7] = 0x40;
		// device
	fFis[8] = (lba >> 24) & 0xff;
	fFis[9] = (lba >> 32) & 0xff;
	fFis[10] = (lba >> 40) & 0xff;
	fFis[12] = sectorCount & 0xff;
	fFis[13] = (sectorCount >> 8) & 0xff;
}


void
sata_request::SetFeature(uint16 feature)
{
	fFis[3] = (uint8)(feature & 0xff);
	fFis[11] = (uint8)(feature >> 8);
}


void
sata_request::SetATAPICommand(size_t transferLength)
{
	fIsATAPI = true;
	SetATACommand(0xa0);
	if (1 /* isPIO */) {
		if (transferLength == 0)
			transferLength = 2;
		else if (transferLength > 0xfffe)
			transferLength = 0xfffe;
		fFis[5] = transferLength & 0xff;
		fFis[6] = (transferLength >> 8) & 0xff;
	}
}


void
sata_request::Finish(int tfd, size_t bytesTransfered)
{
	if ((tfd & (ATA_ERR | ATA_DF)) != 0) {
		uint8 status = tfd & 0xff;
		uint8 error = (tfd >> 8) & 0xff;

		if (!IsTestUnitReady()) {
			dprintf("ahci: sata_request::finish ATA command 0x%02x failed\n",
				fFis[2]);
			dprintf("ahci: sata_request::finish status 0x%02x, error 0x%02x\n",
				status, error);
		}
	}

	if (fCcb) {
		fCcb->data_resid = fCcb->data_length - bytesTransfered;
		fCcb->device_status = SCSI_STATUS_GOOD;
		fCcb->subsys_status = SCSI_REQ_CMP;
		if (tfd & (ATA_ERR | ATA_DF)) {
			fCcb->subsys_status = SCSI_REQ_CMP_ERR;
			if (fIsATAPI) {
				if (!IsTestUnitReady()) {
					dprintf("ahci: sata_request::finish ATAPI packet %02x %02x "
						"%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x "
						"%02x %02x %02x %02x (len %d)\n",
						fCcb->cdb[0], fCcb->cdb[1], fCcb->cdb[2], fCcb->cdb[3],
						fCcb->cdb[4], fCcb->cdb[5], fCcb->cdb[6], fCcb->cdb[7],
						fCcb->cdb[8], fCcb->cdb[9], fCcb->cdb[10],
						fCcb->cdb[11], fCcb->cdb[12], fCcb->cdb[13],
						fCcb->cdb[14], fCcb->cdb[15], fCcb->cdb_length);
				}

				fCcb->device_status = SCSI_STATUS_CHECK_CONDITION;
			} else {
				// TODO ATA error handling goes here
/*
				// TODO check ABORT bit if this is useful
				if ((tfd >> 8) & 0x04) { // ABRT
					fCcb->subsys_status = SCSI_REQ_ABORTED;
				} else {
					fCcb->device_status = SCSI_STATUS_CHECK_CONDITION;
					fCcb->subsys_status |= SCSI_AUTOSNS_VALID;
					fCcb->sense_resid = 0; //FIXME
					scsi_sense *sense = (scsi_sense *)fCcb->sense;
					sense->error_code = SCSIS_CURR_ERROR;
					sense->sense_key = error >> 4;
					sense->asc = 0;
					sense->ascq = 0;
				}
*/
			}
		}
		gSCSI->finished(fCcb, 1);
		delete this;
	} else {
		fCompletionStatus = tfd;
		release_sem(fCompletionSem);
	}
}


void
sata_request::Abort()
{
	dprintf("ahci: sata_request::abort called for command 0x%02x\n", fFis[2]);
	if (fCcb != NULL) {
		fCcb->subsys_status = SCSI_REQ_ABORTED;
		gSCSI->finished(fCcb, 1);
		delete this;
	} else {
		fCompletionStatus = ATA_ERR;
		release_sem(fCompletionSem);
	}
}


void
sata_request::WaitForCompletion()
{
	ASSERT(fCcb == NULL);
	acquire_sem(fCompletionSem);
}


int
sata_request::CompletionStatus()
{
	ASSERT(fCcb == NULL);
	return fCompletionStatus;
}
