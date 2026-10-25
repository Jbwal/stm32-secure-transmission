/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "protocol_icd.h"
#include "string.h"
#include <stdbool.h>
#include<stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t communicationTaskHandle;
osThreadId_t protocolTaskHandle;
osThreadId_t securityTaskHandle;
osThreadId_t statusTaskHandle;

osMessageQueueId_t rxQueue;
osMessageQueueId_t txQueue;

const osThreadAttr_t communicationTask_attributes = {
    .name = "communicationTask",
    .stack_size = 512,
    .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t protocolTask_attributes = {
    .name = "protocolTask",
    .stack_size = 512,
    .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t securityTask_attributes = {
    .name = "securityTask",
    .stack_size = 512,
    .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t statusTask_attributes = {
    .name = "statusTask",
    .stack_size = 512,
    .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

typedef enum
{
    RX_SOURCE_USART1 = 0, /* Trame recue depuis l'autre carte */
    RX_SOURCE_PC = 1       /* Commande recue depuis le terminal USB */
} RxSource_t;

typedef struct
{
    ProtocolMessage_t message;
    RxSource_t source;
} RxQueueItem_t;

typedef enum
{
    MODULE_UNINITIALIZED = 0,
    MODULE_SYNCHRONIZING,
    MODULE_SYNCHRONIZED
} ModuleState_t;

/* Ces informations sont propres a chaque carte et initialisees a l'execution. */
static volatile ModuleState_t moduleState = MODULE_UNINITIALIZED;
static volatile bool moduleIsMaster = false;
static volatile uint8_t modulePosition = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void CommunicationTask(void *argument);
void ProtocolTask(void *argument);
void SecurityTask(void *argument);
void StatusTask(void *argument);
static bool Protocol_IsMessageValid(const ProtocolMessage_t *message);
static void Protocol_ProcessMessage(const RxQueueItem_t *item);
static void Console_ProcessLine(const char *line);
static void Console_Write(const char *text);
static void Protocol_SendUart1(uint8_t command, const uint8_t *payload, uint8_t length);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  rxQueue = osMessageQueueNew(5, sizeof(RxQueueItem_t), NULL);

  txQueue = osMessageQueueNew(5, sizeof(ProtocolMessage_t), NULL);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  communicationTaskHandle =
      osThreadNew(CommunicationTask, NULL, &communicationTask_attributes);

  protocolTaskHandle =
      osThreadNew(ProtocolTask, NULL, &protocolTask_attributes);

  securityTaskHandle =
      osThreadNew(SecurityTask, NULL, &securityTask_attributes);

  statusTaskHandle =
      osThreadNew(StatusTask, NULL, &statusTask_attributes);  

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : USER_BUTTON_Pin */
  GPIO_InitStruct.Pin = USER_BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_BUTTON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */

static void Console_Write(const char *text)
{
    if (text != NULL)
    {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)text,
                                (uint16_t)strlen(text), 100U);
    }
}

static void Console_WriteHexByte(uint8_t value)
{
    static const char hexDigits[] = "0123456789ABCDEF";
    char output[4];

    output[0] = hexDigits[(value >> 4) & 0x0F];
    output[1] = hexDigits[value & 0x0F];
    output[2] = ' ';
    output[3] = '\0';

    Console_Write(output);
}

static void Protocol_SendUart1(uint8_t command, const uint8_t *payload, uint8_t length)
{
    ProtocolMessage_t message = {0};
    HAL_StatusTypeDef status;

    if (length > MAX_PAYLOAD_SIZE)
    {
        Console_Write("ERREUR: payload trop long\r\n");
        return;
    }

    message.header.start_byte = ICD_START_BYTE;
    message.header.command_type = command;
    message.header.length = length;
    message.header.reserve = 0U;
    message.header.id_src = DEFAULT_ID;

    if ((payload != NULL) && (length > 0U))
    {
        memcpy(message.payload, payload, length);
    }

    /* CRC conserve a zero pour cette etape du projet. */
    message.crc = 0U;

    status = HAL_UART_Transmit(&huart1,
                               (uint8_t *)&message,
                               (uint16_t)sizeof(message),
                               100U);

    if (status == HAL_OK)
    {
        Console_Write("Trame envoyee sur USART1 (26 octets).\r\n");
    }
    else
    {
        Console_Write("ERREUR: echec transmission USART1.\r\n");
    }
}

void CommunicationTask(void *argument)
{
    uint8_t rxByte;
    uint8_t frame[sizeof(ProtocolMessage_t)];
    char consoleLine[32];
    size_t consoleIndex = 0U;
    bool discardConsoleLine = false;

    (void)argument;

    Console_Write("Demarrage OK - USART1=liaison cartes, USART2=console PC\r\n");
    Console_Write("Commande disponible: I puis Entree (ou INIT)\r\n");

    for (;;)
    {
        /*
         * USART1 : attendre le marqueur de debut, puis recevoir les 25 octets
         * restants en un seul appel HAL afin d'eviter l'affichage octet par octet.
         */
        if (HAL_UART_Receive(&huart1, &rxByte, 1U, 1U) == HAL_OK)
        {
            if (rxByte == ICD_START_BYTE)
            {
                frame[0] = rxByte;

                if (HAL_UART_Receive(&huart1,
                                     &frame[1],
                                     (uint16_t)(sizeof(frame) - 1U),
                                     20U) == HAL_OK)
                {
                    RxQueueItem_t item = {0};
                    memcpy(&item.message, frame, sizeof(item.message));
                    item.source = RX_SOURCE_USART1;

                    Console_Write("\r\nTrame complete recue sur USART1 : ");
                    for (size_t i = 0U; i < sizeof(frame); i++)
                    {
                        Console_WriteHexByte(frame[i]);
                    }
                    Console_Write("\r\n");

                    if (Protocol_IsMessageValid(&item.message))
                    {
                        if (osMessageQueuePut(rxQueue, &item, 0U, 0U) != osOK)
                        {
                            Console_Write("ERREUR: file RX pleine, trame perdue.\r\n");
                        }
                    }
                    else
                    {
                        Console_Write("ERREUR: trame invalide (entete ou longueur).\r\n");
                    }
                }
                else
                {
                    Console_Write("ERREUR: trame USART1 incomplete (26 octets attendus).\r\n");
                }
            }
        }

        /* USART2 : lire les caracteres du terminal PC sans bloquer l'attente USART1. */
        if (HAL_UART_Receive(&huart2, &rxByte, 1U, 0U) == HAL_OK)
        {
            if ((rxByte == '\r') || (rxByte == '\n'))
            {
                if (!discardConsoleLine && (consoleIndex > 0U))
                {
                    consoleLine[consoleIndex] = '\0';
                    Console_ProcessLine(consoleLine);
                }

                consoleIndex = 0U;
                discardConsoleLine = false;
            }
            else if (!discardConsoleLine)
            {
                char echo[2] = {(char)rxByte, '\0'};
                Console_Write(echo);

                if (consoleIndex < (sizeof(consoleLine) - 1U))
                {
                    consoleLine[consoleIndex++] = (char)rxByte;
                }
                else
                {
                    consoleIndex = 0U;
                    discardConsoleLine = true;
                    Console_Write("\r\nERREUR: commande trop longue\r\n");
                }
            }
        }

        /* Pas de osDelay ici : la boucle doit sonder les UART rapidement. */
    }
}

static void Console_ProcessLine(const char *line)
{
    RxQueueItem_t item = {0};
    char normalized[32];
    size_t i = 0U;
    size_t start = 0U;
    size_t end;

    if (line == NULL)
    {
        return;
    }

    /* Ignore les espaces au debut/fin et convertit la commande en majuscules. */
    while ((line[start] == ' ') || (line[start] == '\t'))
    {
        start++;
    }

    end = strlen(line);
    while ((end > start) &&
           ((line[end - 1U] == ' ') || (line[end - 1U] == '\t')))
    {
        end--;
    }

    while ((start < end) && (i < (sizeof(normalized) - 1U)))
    {
        char c = line[start++];
        if ((c >= 'a') && (c <= 'z'))
        {
            c = (char)(c - 'a' + 'A');
        }
        normalized[i++] = c;
    }
    normalized[i] = '\0';

    Console_Write("\r\nCommande recue: ");
    Console_Write(normalized);
    Console_Write("\r\n");

    if ((strcmp(normalized, "I") == 0) || (strcmp(normalized, "INIT") == 0))
    {
        item.message.header.start_byte = ICD_START_BYTE;
        item.message.header.command_type = CMD_INIT;
        item.message.header.length = 1U;
        item.message.header.reserve = 0U;
        item.message.header.id_src = DEFAULT_ID;
        item.message.payload[0] = 0U; /* INIT = 0 selon l'ICD */
        item.message.crc = 0U;
        item.source = RX_SOURCE_PC;

        if (osMessageQueuePut(rxQueue, &item, 0U, 0U) != osOK)
        {
            Console_Write("ERREUR: impossible de mettre INIT en file\r\n");
        }
        else
        {
            Console_Write("INIT ajoute a la file de traitement\r\n");
        }
    }
    else if (normalized[0] != '\0')
    {
        Console_Write("Commande inconnue. Envoyer I puis Entree.\r\n");
    }
}

static bool Protocol_IsMessageValid(const ProtocolMessage_t *message)
{
    if (message == NULL)
    {
        return false;
    }

    /* Verifications structurelles communes a toutes les commandes. */
    if (message->header.start_byte != ICD_START_BYTE)
    {
        return false;
    }

    if (message->header.reserve != 0U)
    {
        return false;
    }

    if (message->header.length > MAX_PAYLOAD_SIZE)
    {
        return false;
    }

    /* Refuser les valeurs de commande qui ne sont pas definies dans l'ICD. */
    if ((message->header.command_type != CMD_INIT)  &&
        (message->header.command_type != CMD_RESET) &&
        (message->header.command_type != CMD_ACK)   &&
        (message->header.command_type != CMD_MSG)   &&
        (message->header.command_type != CMD_ERROR))
    {
        return false;
    }

    /* Verifications propres aux commandes deja utilisees par le test. */
    if (message->header.command_type == CMD_INIT)
    {
        if ((message->header.length != 1U) || (message->payload[0] != 0U))
        {
            return false;
        }
    }
    else if (message->header.command_type == CMD_ACK)
    {
        if (message->header.length != 0U)
        {
            return false;
        }
    }

    /* Le CRC est encore fixe a zero dans cette version et n'est pas verifie. */
    return true;
}

static void Protocol_ProcessMessage(const RxQueueItem_t *item)
{
    const ProtocolMessage_t *message;

    if (item == NULL)
    {
        return;
    }
    message = &item->message;

    switch (message->header.command_type)
    {
        case CMD_INIT:
        {
            if ((message->header.length < 1U) || (message->payload[0] != 0U))
            {
                Console_Write("INIT ignore: payload invalide\r\n");
                break;
            }

            if (item->source == RX_SOURCE_PC)
            {
                /* Une commande INIT saisie sur le PC designe cette carte comme maitre. */
                if (moduleState == MODULE_UNINITIALIZED)
                {
                    moduleIsMaster = true;
                    modulePosition = 1U;
                    moduleState = MODULE_SYNCHRONIZING;
                    Console_Write("INIT PC recue: role MAITRE, position 1\r\n");
                    Console_Write("INIT transmis a la carte voisine; attente ACK\r\n");
                    Protocol_SendUart1(CMD_INIT, message->payload, 1U);
                }
                else if (moduleIsMaster)
                {
                    Console_Write("Carte deja initialisee comme MAITRE\r\n");
                }
                else
                {
                    Console_Write("INIT refuse: carte deja initialisee comme ESCLAVE\r\n");
                }
            }
            else
            {
                /* INIT venant de la carte precedente: cette carte devient esclave. */
                if (moduleState == MODULE_UNINITIALIZED)
                {
                    moduleIsMaster = false;
                    modulePosition = 2U; /* Topologie de test a deux cartes. */
                    moduleState = MODULE_SYNCHRONIZED;
                    Console_Write("INIT USART1 recue: role ESCLAVE, position 2\r\n");
                }

                /* L'ACK confirme la reception de la commande INIT au maitre. */
                Protocol_SendUart1(CMD_ACK, NULL, 0U);
                Console_Write("ACK renvoye au maitre\r\n");
            }
            break;
        }

        case CMD_ACK:
            if (moduleIsMaster && (moduleState == MODULE_SYNCHRONIZING))
            {
                moduleState = MODULE_SYNCHRONIZED;
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
                Console_Write("ACK recu: chaine synchronisee (test 2 cartes)\r\n");
            }
            else
            {
                Console_Write("ACK recu\r\n");
            }
            break;

        case CMD_RESET:
            Console_Write("Commande RESET recue (traitement a implementer)\r\n");
            break;

        case CMD_MSG:
            Console_Write("Commande MESSAGE recue (traitement a implementer)\r\n");
            break;

        case CMD_ERROR:
            Console_Write("Commande ERROR recue\r\n");
            break;

        default:
            Console_Write("Type de commande inconnu\r\n");
            break;
    }
}

void ProtocolTask(void *argument)
{
    RxQueueItem_t item;
    (void)argument;

    for (;;)
    {
        if (osMessageQueueGet(rxQueue, &item, NULL, osWaitForever) == osOK)
        {
            if (Protocol_IsMessageValid(&item.message))
            {
                Protocol_ProcessMessage(&item);
            }
        }
    }
}

void SecurityTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        /* La generation et la distribution de cle seront ajoutees a l'etape suivante. */
        osDelay(10U);
    }
}

void StatusTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        /* La gestion complete des indicateurs sera ajoutee plus tard. */
        osDelay(10U);
    }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
