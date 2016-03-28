#include <stdint.h>

#ifndef __IR_CLUSTER_H
#define __IR_CLUSTER_H

/* search for (one-dimensional) clusters within data[],
 * consider only values at even positions */
uint8_t cluster(uint16_t data[], uint8_t len, uint16_t cluster[], uint8_t max);

/* get cluster index belonging to data */
uint8_t min_cluster(uint16_t data, uint16_t cluster[], uint8_t len);

#endif
