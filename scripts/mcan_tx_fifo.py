#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate the red panda M_CAN FIFO driver from the pinned Zephyr source."""

from pathlib import Path
import sys


def fifo_source(source: str) -> str:
    replacements = [
        (
            "config->mram_elements[CAN_MCAN_MRAM_CFG_TX_BUFFER]) | CAN_MCAN_TXBC_TFQM;",
            "config->mram_elements[CAN_MCAN_MRAM_CFG_TX_BUFFER]);",
        ),
        (
            """	/* Acquire a free TX buffer */
	for (int i = 0; i < cbs->num_tx; i++) {
		if (cbs->tx[i].function == NULL) {
			put_idx = i;
			break;
		}
	}

	/* A free TX buffer should always be available since the data->tx_sem was acquired */
	__ASSERT_NO_MSG(put_idx < cbs->num_tx);""",
            """	/* FIFO mode requires the hardware put index, including after wraparound.
	 * A lower-numbered free buffer may belong to a different FIFO position.
	 */
	err = can_mcan_read_reg(dev, CAN_MCAN_TXFQS, &reg);
	if (err != 0) {
		goto unlock;
	}

	put_idx = FIELD_GET(CAN_MCAN_TXFQS_TFQPI, reg);
	if ((reg & CAN_MCAN_TXFQS_TFQF) != 0U || put_idx >= cbs->num_tx ||
	    cbs->tx[put_idx].function != NULL) {
		err = -EAGAIN;
		goto unlock;
	}""",
        ),
    ]
    for old, new in replacements:
        if source.count(old) != 1:
            raise ValueError("Zephyr M_CAN source changed; review the local FIFO fix")
        source = source.replace(old, new, 1)
    return source


def main() -> None:
    source, destination = map(Path, sys.argv[1:])
    result = fifo_source(source.read_text())
    if not destination.exists() or destination.read_text() != result:
        destination.write_text(result)


if __name__ == "__main__":
    main()
