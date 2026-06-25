#include <stdint.h>
#include "esc_coe.h"
#include "galvo.h"

galvo_rxpdo_t GalvoRxPdo;
galvo_txpdo_t GalvoTxPdo;

static uint8_t obj1c12_sub0 = 1;
static uint16_t obj1c12_sub1 = 0x1600;
static uint8_t obj1c13_sub0 = 1;
static uint16_t obj1c13_sub1 = 0x1A00;

static const _objd obj1000[] =
{
   {0, DTYPE_UNSIGNED32, 32, ATYPE_RO, "Device type", 0x00001389, 0}
};

static const _objd obj1008[] =
{
   {0, DTYPE_VISIBLE_STRING, 8 * 18, ATYPE_RO, "Device name", 0, "EtherCAT GalvoCtrl"}
};

static const _objd obj1018[] =
{
   {0, DTYPE_UNSIGNED8, 8, ATYPE_RO, "SubIndex 000", 4, 0},
   {1, DTYPE_UNSIGNED32, 32, ATYPE_RO, "Vendor ID", 0x00000000, 0},
   {2, DTYPE_UNSIGNED32, 32, ATYPE_RO, "Product code", 0x00000001, 0},
   {3, DTYPE_UNSIGNED32, 32, ATYPE_RO, "Revision", 0x00000001, 0},
   {4, DTYPE_UNSIGNED32, 32, ATYPE_RO, "Serial number", 0x00000000, 0}
};

static const _objd obj1600[] =
{
   {0, DTYPE_UNSIGNED8, 8, ATYPE_RO, "SubIndex 000", 7, 0},
   {1, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Control word", 0x70000110, 0},
   {2, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Start X", 0x70000220, 0},
   {3, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Start Y", 0x70000320, 0},
   {4, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "End X", 0x70000420, 0},
   {5, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "End Y", 0x70000520, 0},
   {6, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Cycle counter", 0x70000620, 0},
   {7, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Flags", 0x70000710, 0}
};

static const _objd obj1a00[] =
{
   {0, DTYPE_UNSIGNED8, 8, ATYPE_RO, "SubIndex 000", 10, 0},
   {1, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Status word", 0x60000110, 0},
   {2, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Start X", 0x60000220, 0},
   {3, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Start Y", 0x60000320, 0},
   {4, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "End X", 0x60000420, 0},
   {5, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "End Y", 0x60000520, 0},
   {6, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Actual X", 0x60000620, 0},
   {7, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Actual Y", 0x60000720, 0},
   {8, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Following error X", 0x60000820, 0},
   {9, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Following error Y", 0x60000920, 0},
   {10, DTYPE_PDO_MAPPING, 32, ATYPE_RO, "Sequence", 0x60000A20, 0}
};

static const _objd obj1c12[] =
{
   {0, DTYPE_UNSIGNED8, 8, ATYPE_RWpre, "SubIndex 000", 1, &obj1c12_sub0},
   {1, DTYPE_UNSIGNED16, 16, ATYPE_RWpre, "RxPDO assign", 0x1600, &obj1c12_sub1}
};

static const _objd obj1c13[] =
{
   {0, DTYPE_UNSIGNED8, 8, ATYPE_RWpre, "SubIndex 000", 1, &obj1c13_sub0},
   {1, DTYPE_UNSIGNED16, 16, ATYPE_RWpre, "TxPDO assign", 0x1A00, &obj1c13_sub1}
};

static const _objd obj6000[] =
{
   {0, DTYPE_UNSIGNED8, 8, ATYPE_RO, "SubIndex 000", 10, 0},
   {1, DTYPE_UNSIGNED16, 16, ATYPE_RO | ATYPE_TXPDO, "Status word", 0, &GalvoTxPdo.status},
   {2, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, "Start X", 0, &GalvoTxPdo.start_x},
   {3, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, "Start Y", 0, &GalvoTxPdo.start_y},
   {4, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, "End X", 0, &GalvoTxPdo.end_x},
   {5, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, "End Y", 0, &GalvoTxPdo.end_y},
   {6, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, "Actual X", 0, &GalvoTxPdo.actual_x},
   {7, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, "Actual Y", 0, &GalvoTxPdo.actual_y},
   {8, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, "Following error X", 0, &GalvoTxPdo.following_error_x},
   {9, DTYPE_INTEGER32, 32, ATYPE_RO | ATYPE_TXPDO, "Following error Y", 0, &GalvoTxPdo.following_error_y},
   {10, DTYPE_UNSIGNED32, 32, ATYPE_RO | ATYPE_TXPDO, "Sequence", 0, &GalvoTxPdo.sequence}
};

static const _objd obj7000[] =
{
   {0, DTYPE_UNSIGNED8, 8, ATYPE_RO, "SubIndex 000", 7, 0},
   {1, DTYPE_UNSIGNED16, 16, ATYPE_RW | ATYPE_RXPDO, "Control word", 0, &GalvoRxPdo.control},
   {2, DTYPE_INTEGER32, 32, ATYPE_RW | ATYPE_RXPDO, "Start X", 0, &GalvoRxPdo.start_x},
   {3, DTYPE_INTEGER32, 32, ATYPE_RW | ATYPE_RXPDO, "Start Y", 0, &GalvoRxPdo.start_y},
   {4, DTYPE_INTEGER32, 32, ATYPE_RW | ATYPE_RXPDO, "End X", 0, &GalvoRxPdo.end_x},
   {5, DTYPE_INTEGER32, 32, ATYPE_RW | ATYPE_RXPDO, "End Y", 0, &GalvoRxPdo.end_y},
   {6, DTYPE_UNSIGNED32, 32, ATYPE_RW | ATYPE_RXPDO, "Cycle counter", 0, &GalvoRxPdo.cycle_counter},
   {7, DTYPE_UNSIGNED16, 16, ATYPE_RW | ATYPE_RXPDO, "Flags", 0, &GalvoRxPdo.flags}
};

const _objectlist SDOobjects[] =
{
   {0x1000, OTYPE_VAR, 0, 0, "Device type", obj1000},
   {0x1008, OTYPE_VAR, 0, 0, "Device name", obj1008},
   {0x1018, OTYPE_RECORD, 4, 0, "Identity object", obj1018},
   {0x1600, OTYPE_RECORD, 7, 0, "RxPDO mapping", obj1600},
   {0x1A00, OTYPE_RECORD, 10, 0, "TxPDO mapping", obj1a00},
   {0x1C12, OTYPE_ARRAY, 1, 0, "RxPDO assign", obj1c12},
   {0x1C13, OTYPE_ARRAY, 1, 0, "TxPDO assign", obj1c13},
   {0x6000, OTYPE_RECORD, 10, 0, "Galvo inputs", obj6000},
   {0x7000, OTYPE_RECORD, 7, 0, "Galvo outputs", obj7000},
   {0xffff, 0, 0, 0, "End", 0}
};
