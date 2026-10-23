export interface ParsedHex {
  baseAddress: number;
  totalBytes: number;
  data: Uint8Array;
}

/**
 * Robust Intel HEX Parser
 * Supports Record Types:
 *   00 - Data Record
 *   01 - End of File (EOF)
 *   02 - Extended Segment Address Record
 *   04 - Extended Linear Address Record (upper 16 bits of 32-bit address)
 * Gaps in address ranges are automatically filled with 0xFF (flash erased state).
 */
export function parseIntelHex(hexText: string): ParsedHex {
  const lines = hexText.split(/\r?\n/);
  let upperAddress = 0;
  let minAddress = 0xFFFFFFFF;
  let maxAddress = 0x00000000;
  const memoryMap = new Map<number, number>();

  for (let line of lines) {
    line = line.trim();
    if (!line.startsWith(':') || line.length < 11) continue;

    const byteCount = parseInt(line.substring(1, 3), 16);
    const address = parseInt(line.substring(3, 7), 16);
    const recordType = parseInt(line.substring(7, 9), 16);

    if (recordType === 0x00) {
      // Data Record
      const fullAddress = (upperAddress << 16) | address;
      for (let i = 0; i < byteCount; i++) {
        const byteVal = parseInt(line.substring(9 + i * 2, 11 + i * 2), 16);
        const addr = fullAddress + i;
        memoryMap.set(addr, byteVal);
        if (addr < minAddress) minAddress = addr;
        if (addr > maxAddress) maxAddress = addr;
      }
    } else if (recordType === 0x01) {
      // EOF Record
      break;
    } else if (recordType === 0x04) {
      // Extended Linear Address Record (upper 16 bits)
      upperAddress = parseInt(line.substring(9, 13), 16);
    } else if (recordType === 0x02) {
      // Extended Segment Address Record
      upperAddress = (parseInt(line.substring(9, 13), 16) << 4) >> 16;
    }
  }

  if (memoryMap.size === 0) {
    return { baseAddress: 0x08000000, totalBytes: 0, data: new Uint8Array(0) };
  }

  const totalBytes = (maxAddress - minAddress) + 1;
  const binaryData = new Uint8Array(totalBytes);
  binaryData.fill(0xFF); // Unprogrammed flash is 0xFF

  for (const [addr, val] of memoryMap.entries()) {
    binaryData[addr - minAddress] = val;
  }

  return {
    baseAddress: minAddress,
    totalBytes: totalBytes,
    data: binaryData
  };
}

