# Secure Transmission Chain

## 1. Project Overview

This project implements a secure communication chain using STM32
microcontrollers.

The objective is to transmit messages between an entry point and an
output point through intermediate modules while ensuring:

- Confidentiality
- Message integrity
- Module identification
- Initialization and reset management

The project is developed as part of an embedded systems engineering
course.

## 2. Hardware

- Microcontroller: STM32 Nucleo-F446RE
- Development environment: STM32CubeMX
- RTOS: FreeRTOS
- Communication interface: UART / USART

Two Nucleo-F446RE boards are available for development and testing.

## 3. Software Architecture

The application is organized into four main tasks:

| Task | Responsibility |
|------|----------------|
| CommunicationTask | Communication management |
| ProtocolTask | Protocol and frame processing |
| SecurityTask | Security mechanisms |
| StatusTask | System state management |

Inter-task communication uses FreeRTOS queues.

## 4. Development Roadmap

- [x] Initial STM32CubeMX project setup
- [ ] Define and validate the communication frame format
- [ ] Implement UART communication between two boards
- [ ] Implement protocol commands and responses
- [ ] Implement module initialization and identification
- [ ] Implement confidentiality and integrity mechanisms
- [ ] Implement system status indicators
- [ ] Test the complete communication chain

## 5. Repository Structure

```text
Projet_Chaine_De_Transmission/
├── Core/
│   ├── Inc/
│   └── Src/
├── Drivers/
├── Middlewares/
├── Projet_Chaine_De_Transmission.ioc
└── README.md