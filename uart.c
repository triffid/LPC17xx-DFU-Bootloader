#include "uart.h"

#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_clkpwr.h"

// #include "debug.h"

// void dbgled(int l);
// void setled(int, int);

#ifndef ENTER_ISR
#define ENTER_ISR() do {} while (0)
#endif

#ifndef LEAVE_ISR
#define LEAVE_ISR() do {} while (0)
#endif

/* Buf mask */
#define __BUF_MASK (UART_RING_BUFSIZE-1)
/* Check buf is full or not */
#define __BUF_IS_FULL(head, tail) ((tail&__BUF_MASK)==((head+1)&__BUF_MASK))
/* Check buf will be full in next receiving or not */
#define __BUF_WILL_FULL(head, tail) ((tail&__BUF_MASK)==((head+2)&__BUF_MASK))
/* Check buf is empty */
#define __BUF_IS_EMPTY(head, tail) ((head&__BUF_MASK)==(tail&__BUF_MASK))
/* Reset buf */
#define __BUF_RESET(bufidx)     (bufidx=0)
#define __BUF_INCR(bufidx)      (bufidx=(bufidx+1)&__BUF_MASK)

#define RB_MASK          (UART_RINGBUFFER_SIZE - 1)
#define RB_FULL(rb)      ((rb.tail & RB_MASK) == ((rb.head + 1) & RB_MASK))
#define RB_EMPTY(rb)     ((rb.head & RB_MASK) == ( rb.tail      & RB_MASK))
#define RB_ZERO(rb)      do { rb.head = rb.tail = 0; } while (0)
#define RB_INCR(ht)      do { ht = (ht + 1) & RB_MASK; } while (0)
#define RB_PUSH(rb, val) do { rb.data[rb.head++] = val; rb.head &= RB_MASK; } while (0)
#define RB_POP( rb, val) do { val = rb.data[rb.tail++]; rb.tail &= RB_MASK; } while (0)
#define RB_PEEK(rb, val) do { val = rb.data[rb.tail++]; } while (0)
#define RB_DROP(rb)      do { rb.tail = (rb.tail + 1) & RB_MASK; } while (0)
#define RB_CANREAD(rb)   (((UART_RINGBUFFER_SIZE + rb.head) -  rb.tail     ) & RB_MASK)
#define RB_CANWRITE(rb)  (((UART_RINGBUFFER_SIZE + rb.tail) - (rb.head + 1)) & RB_MASK)

LPC_UART_TypeDef * u;
// UART Ring buffers
UART_RINGBUFFER_T txbuf;
UART_RINGBUFFER_T rxbuf;

// Current Tx Interrupt enable state
__IO FlagStatus TxIntStat;
volatile uint8_t blocking;
#define true 1
#define false 0

int port;



// UART_UART(PinName rxpin, PinName txpin)
// {
//     pin_init(rxpin, txpin);
// }

void UART_init(PinName rxpin, PinName txpin, int baud)
{
    UART_pin_init(rxpin, txpin);
    UART_baud(baud);
}

void UART_pin_init(PinName rxpin, PinName txpin)
{
    blocking = true;

    PINSEL_CFG_Type PinCfg;

    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;

    if (txpin == P0_2 && rxpin == P0_3) {
        port = 0;
        u = LPC_UART0;
        PinCfg.Funcnum = 1;
    }
    else if (txpin == P0_0 && rxpin == P0_1) {
        port = 3;
        u = LPC_UART3;
        PinCfg.Funcnum = 2;
    }
    else if (txpin == P0_10 && rxpin == P0_11) {
        port = 2;
        u = LPC_UART2;
        PinCfg.Funcnum = 1;
    }
    else if (txpin == P0_15 && rxpin == P0_16) {
        port = 1;
        u = (LPC_UART_TypeDef *) LPC_UART1;
        PinCfg.Funcnum = 1;
    }
    else if (txpin == P0_25 && rxpin == P0_26) {
        port = 3;
        u = LPC_UART3;
        PinCfg.Funcnum = 3;
    }
    else if (txpin == P2_0 && rxpin == P2_1) {
        port = 1;
        u = (LPC_UART_TypeDef *) LPC_UART1;
        PinCfg.Funcnum = 2;
    }
    else if (txpin == P2_8 && rxpin == P2_9) {
        port = 2;
        u = LPC_UART2;
        PinCfg.Funcnum = 2;
    }
    else if (txpin == P4_28 && rxpin == P4_29) {
        port = 3;
        u = LPC_UART3;
        PinCfg.Funcnum = 3;
    }
    else {
        //TODO: software serial
        port = -1;
        return;
    }

    PinCfg.Portnum = (txpin >> 5) & 7;
    PinCfg.Pinnum = (txpin & 0x1F);
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Portnum = (rxpin >> 5) & 7;
    PinCfg.Pinnum = (rxpin & 0x1F);
    PINSEL_ConfigPin(&PinCfg);

//     irqrouter[port] = this;
}

int UART_baud(int baud)
{
    TxIntStat = RESET;

    RB_ZERO(txbuf);
    RB_ZERO(rxbuf);

    UART_CFG_Type UARTConfigStruct;
    UART_FIFO_CFG_Type UARTFIFOConfigStruct;

    UART_ConfigStructInit(&UARTConfigStruct);

    UARTConfigStruct.Baud_rate = baud;

    UART_Init(u, &UARTConfigStruct);

    UART_FIFOConfigStructInit(&UARTFIFOConfigStruct);

    UART_FIFOConfig(u, &UARTFIFOConfigStruct);

    UART_TxCmd(u, ENABLE);

    UART_IntConfig(u, UART_INTCFG_RBR, ENABLE);
    UART_IntConfig(u, UART_INTCFG_RLS, ENABLE);

    IRQn_Type c;
    switch (port) {
        case 0:
            c = UART0_IRQn;
            break;
        case 1:
            c = UART1_IRQn;
            break;
        case 2:
            c = UART2_IRQn;
            break;
        case 3:
            c = UART3_IRQn;
            break;
        default:
            return baud;
    }

    NVIC_EnableIRQ(c);

    return baud;
}

uint32_t UART_send(const uint8_t *data, uint32_t buflen) {
//     uint8_t *data = (uint8_t *) buf;
    uint32_t bytes = 0;

    // only fiddle interrupt status outside interrupt context
    uint8_t intr = __get_IPSR() & 0x1F;

    if (intr == 0) __disable_irq();

    while ((buflen > 0) && (!RB_FULL(txbuf) || blocking))
    {
        if (intr == 0) __enable_irq();

        if (RB_FULL(txbuf)) {
            if (blocking && (intr == 0))
            {
                while (RB_FULL(txbuf))
                    __WFI();
            }
            else {
                RB_DROP(txbuf);
            }
        }

        if (intr == 0) __disable_irq();

        RB_PUSH(txbuf, *data++);

        if (intr == 0) __enable_irq();

        bytes++;
        buflen--;

        if (intr == 0) __disable_irq();
    }
    if (intr == 0) __enable_irq();

    if (TxIntStat == RESET) {
        UART_tx_isr();
    }

    return bytes;
}

// uint32_t UART_send(const char *buf, uint32_t buflen) {
//     return send((uint8_t *) buf, buflen);
// }

uint32_t UART_recv(uint8_t *buf, uint32_t buflen) {
    uint32_t bytes = 0;

    // only fiddle interrupt status outside interrupt context
    uint8_t intr = __get_IPSR() & 0x1F;

    if (intr == 0) __disable_irq();

    while ((buflen > 0) && (!(RB_EMPTY(rxbuf))))
    {
        RB_POP(rxbuf, *buf++);

        if (intr == 0) __enable_irq();

        bytes++;
        buflen--;

        if (intr == 0) __disable_irq();
    }
    if (intr == 0) __enable_irq();

    return bytes;
}

int UART_cansend() {
    return RB_CANWRITE(txbuf);
}

int UART_canrecv() {
    return RB_CANREAD(rxbuf);
}

void UART_isr()
{
    uint32_t intsrc, ls;

    /* Determine the interrupt source */
    intsrc = UART_GetIntId(u) & UART_IIR_INTID_MASK;

    // Receive Line Status
    if (intsrc == UART_IIR_INTID_RLS)
    {
        // Check line status
        ls = UART_GetLineStatus(u);
        // Mask out the Receive Ready and Transmit Holding empty status
        ls &= (UART_LSR_OE | UART_LSR_PE | UART_LSR_FE | UART_LSR_BI | UART_LSR_RXFE);
        // If any error exist
        if (ls)
        {
            UART_err_isr(ls & 0xFF);
        }
    }

    // Receive Data Available or Character time-out
    if ((intsrc == UART_IIR_INTID_RDA) || (intsrc == UART_IIR_INTID_CTI))
    {
        UART_rx_isr();
    }

    // Transmit Holding Empty
    if (intsrc == UART_IIR_INTID_THRE)
    {
        UART_tx_isr();
    }
}

void UART_tx_isr() {
    // Disable THRE interrupt
    UART_IntConfig(u, UART_INTCFG_THRE, DISABLE);

    /* Wait for FIFO buffer empty, transfer UART_TX_FIFO_SIZE bytes
     * of data or break whenever ring buffers are empty */
    /* Wait until THR empty */
    while (UART_CheckBusy(u) == SET);

    while (!RB_EMPTY(txbuf))
    {
        /* Move a piece of data into the transmit FIFO */
        if (UART_Send(u, (uint8_t *)&txbuf.data[txbuf.tail], 1, NONE_BLOCKING)){
            /* Update transmit ring FIFO tail pointer */
//             __BUF_INCR(txbuf.tail);
            RB_DROP(txbuf);
        } else {
            break;
        }
    }

    /* If there is no more data to send, disable the transmit
     *       interrupt - else enable it or keep it enabled */
    if (RB_EMPTY(txbuf)) {
        UART_IntConfig(u, UART_INTCFG_THRE, DISABLE);
        // Reset Tx Interrupt state
        TxIntStat = RESET;
    }
    else{
        // Set Tx Interrupt state
        TxIntStat = SET;
        UART_IntConfig(u, UART_INTCFG_THRE, ENABLE);
    }
}

void UART_rx_isr() {
    uint8_t c;
    uint32_t r;

    while(1){
        // Call UART read function in UART driver
        r = UART_Receive(u, &c, 1, NONE_BLOCKING);
        // If data received
        if (r){
            /* Check if buffer is more space
             * If no more space, remaining character will be trimmed out
             */
            if (!RB_FULL(rxbuf)){
//                 rxbuf.data[rxbuf.head] = tmpc;
//                 __BUF_INCR(rxbuf.head);
                RB_PUSH(rxbuf, c);
            }
        }
        // no more data
        else {
            break;
        }
    }
}

void UART_err_isr(uint8_t bLSErrType) {
    uint8_t test;
    // Loop forever
    while (1){
        // For testing purpose
        test = bLSErrType;
    }
}

// extern "C" {
void UART0_IRQHandler(void)
{
	ENTER_ISR();
// 	if (UART_irqrouter[0])
// 		UART_irqrouter[0]->isr();
	UART_isr();
	LEAVE_ISR();
}
/*
void UART1_IRQHandler(void)
{
	ENTER_ISR();
	if (UART_irqrouter[1])
		UART_irqrouter[1]->isr();
	LEAVE_ISR();
}

void UART2_IRQHandler(void)
{
	ENTER_ISR();
	if (UART_irqrouter[2])
		UART_irqrouter[2]->isr();
	LEAVE_ISR();
}

void UART3_IRQHandler(void)
{
	ENTER_ISR();
	if (UART_irqrouter[3])
		UART_irqrouter[3]->isr();
	LEAVE_ISR();
}
// }*/
