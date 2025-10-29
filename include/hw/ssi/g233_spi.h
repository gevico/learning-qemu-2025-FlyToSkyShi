#ifndef HW_G233_SPI_H
#define HW_G233_SPI_H

#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "qemu/timer.h"
#include "qemu/fifo8.h"
#include "qom/object.h"

#define TYPE_G233_SPI "g233-spi"
OBJECT_DECLARE_SIMPLE_TYPE(G233SPIState, G233_SPI)

/* Register offsets */
#define SPI_CR1     0x00
#define SPI_CR2     0x04  
#define SPI_SR      0x08
#define SPI_DR      0x0C
#define SPI_CSCTRL  0x10

#define SPI_REG_SIZE 0x400

/* SPI_CR1 bits */
#define SPI_CR1_SPE  (1 << 6)
#define SPI_CR1_MSTR (1 << 2)

/* SPI_CR2 bits */
#define SPI_CR2_TXEIE  (1 << 7)
#define SPI_CR2_RXNEIE (1 << 6) 
#define SPI_CR2_ERRIE  (1 << 5)
#define SPI_CR2_SSOE   (1 << 4)

/* SPI_SR bits */
#define SPI_SR_BSY      (1 << 7)
#define SPI_SR_OVERRUN  (1 << 3)
#define SPI_SR_UNDERRUN (1 << 2)
#define SPI_SR_TXE      (1 << 1)
#define SPI_SR_RXNE     (1 << 0)

struct G233SPIState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    SSIBus *ssi;

    /* Registers */
    uint32_t cr1;
    uint32_t cr2;
    uint32_t sr;
    uint32_t dr;
    uint32_t csctrl;

    /* CS lines */
    qemu_irq cs_lines[4];
};

#endif
