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
    .stack_size = 2048,
    .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t protocolTask_attributes = {
    .name = "protocolTask",
    .stack_size = 2048,
    /* Prioritaire sur CommunicationTask pour traiter immediatement rxQueue. */
    .priority = (osPriority_t) osPriorityAboveNormal,
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

/* Cle de 32 bits stockee localement sur chaque carte. */
static uint32_t moduleKey = 0U;
static volatile bool keyUpdatePending = false;

/* Reinitialisation coordonnee et triple-clic sur le bouton USER. */
static volatile bool resetPending = false;
static volatile bool buttonResetRequested = false;
static volatile uint8_t userButtonPressCount = 0U;
static volatile uint32_t userButtonFirstPressTick = 0U;
static volatile uint32_t userButtonLastPressTick = 0U;

/* Reception USART1 et USART2 par interruption avec buffers circulaires. */
#define UART1_RX_RING_SIZE 128U
#define UART2_RX_RING_SIZE 128U

static uint8_t uart1RxByte;
static uint8_t uart1RxRing[UART1_RX_RING_SIZE];
static volatile uint16_t uart1RxHead = 0U;
static volatile uint16_t uart1RxTail = 0U;
static volatile uint32_t uart1RxDroppedBytes = 0U;

static uint8_t uart2RxByte;
static uint8_t uart2RxRing[UART2_RX_RING_SIZE];
static volatile uint16_t uart2RxHead = 0U;
static volatile uint16_t uart2RxTail = 0U;
static volatile uint32_t uart2RxDroppedBytes = 0U;

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
static void Console_WriteHexByte(uint8_t value);
static void Protocol_SendUart1(uint8_t command, const uint8_t *payload, uint8_t length);
static uint32_t Protocol_GenerateKey32(void);
static void Protocol_XorCrypt(uint8_t *data, uint8_t length, uint32_t key);

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

  if ((rxQueue == NULL) || (txQueue == NULL))
  {
      Error_Handler();
  }
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
    size_t frameIndex = 0U;
    uint32_t lastFrameByteTick = 0U;
    char consoleLine[32];
    size_t consoleIndex = 0U;
    bool discardConsoleLine = false;
    HAL_StatusTypeDef rxStatus;

    (void)argument;

    Console_Write("Demarrage OK - USART1=interruptions, USART2=console PC\r\n");
    Console_Write("Commandes: INIT, REINITIALISATION, INFO, GET_INFO, GET_KEY, texte (max 16 octets)\r\n");

    /* Demarrer les receptions interruptives sur les deux UART. */
    rxStatus = HAL_UART_Receive_IT(&huart1, &uart1RxByte, 1U);
    if (rxStatus != HAL_OK)
    {
        Console_Write("ERREUR: impossible de demarrer la reception USART1 par interruption.\r\n");
    }

    rxStatus = HAL_UART_Receive_IT(&huart2, &uart2RxByte, 1U);
    if (rxStatus != HAL_OK)
    {
        Console_Write("ERREUR: impossible de demarrer la reception USART2 par interruption.\r\n");
    }

    for (;;)
    {
        /* Traiter rapidement tous les octets USART1 mis en tampon par l'interruption. */
        while (uart1RxTail != uart1RxHead)
        {
            rxByte = uart1RxRing[uart1RxTail];
            uart1RxTail = (uint16_t)((uart1RxTail + 1U) % UART1_RX_RING_SIZE);

            if (frameIndex == 0U)
            {
                if (rxByte == ICD_START_BYTE)
                {
                    frame[0] = rxByte;
                    frameIndex = 1U;
                    lastFrameByteTick = HAL_GetTick();
                }
            }
            else
            {
                frame[frameIndex++] = rxByte;
                lastFrameByteTick = HAL_GetTick();

                if (frameIndex == sizeof(frame))
                {
                    RxQueueItem_t item = {0};
                    memcpy(&item.message, frame, sizeof(item.message));
                    item.source = RX_SOURCE_USART1;
                    frameIndex = 0U;

                    if (osMessageQueuePut(rxQueue, &item, 0U, 0U) != osOK)
                    {
                        Console_Write("ERREUR: file RX pleine, trame perdue.\r\n");
                    }
                }
            }
        }

        /* Diagnostic si une trame commence mais ne se termine pas. */
        if ((frameIndex > 0U) && ((HAL_GetTick() - lastFrameByteTick) > 100U))
        {
            char debugMessage[96];
            (void)snprintf(debugMessage, sizeof(debugMessage),
                           "\r\nERREUR: trame USART1 incomplete, %lu/%lu octets recus.\r\n",
                           (unsigned long)frameIndex, (unsigned long)sizeof(frame));
            Console_Write(debugMessage);
            Console_Write("Debut de trame: ");
            for (size_t i = 0U; (i < frameIndex) && (i < 8U); i++)
            {
                Console_WriteHexByte(frame[i]);
            }
            Console_Write("\r\n");
            frameIndex = 0U;
        }

        /*
         * USART2 : traiter tous les octets de console captures par interruption.
         * On vide le ring buffer, pas un seul octet par tour de boucle.
         */
        while (uart2RxTail != uart2RxHead)
        {
            rxByte = uart2RxRing[uart2RxTail];
            uart2RxTail = (uint16_t)((uart2RxTail + 1U) % UART2_RX_RING_SIZE);

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

        /* Convertir le triple-clic en commande protocolaire hors interruption. */
        if (buttonResetRequested)
        {
            RxQueueItem_t buttonItem = {0};
            buttonResetRequested = false;
            buttonItem.message.header.start_byte = ICD_START_BYTE;
            buttonItem.message.header.command_type = CMD_RESET;
            buttonItem.message.header.length = 1U;
            buttonItem.message.header.reserve = 0U;
            buttonItem.message.header.id_src = DEFAULT_ID;
            buttonItem.message.payload[0] = 0U;
            buttonItem.message.crc = 0U;
            buttonItem.source = RX_SOURCE_PC;

            if (osMessageQueuePut(rxQueue, &buttonItem, 0U, 0U) == osOK)
            {
                Console_Write("\r\nTriple appui USER detecte: reinitialisation demandee.\r\n");
            }
            else
            {
                Console_Write("\r\nERREUR: file RX pleine; reinitialisation non lancee.\r\n");
            }
        }

        osDelay(1U);
    }
}

/*
 * Reception USART1 par interruption. Cette routine ne fait aucun affichage
 * et n'appelle pas FreeRTOS : elle depose seulement l'octet dans le ring buffer.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if ((huart != NULL) && (huart->Instance == USART1))
    {
        uint16_t nextHead = (uint16_t)((uart1RxHead + 1U) % UART1_RX_RING_SIZE);
        if (nextHead != uart1RxTail)
        {
            uart1RxRing[uart1RxHead] = uart1RxByte;
            uart1RxHead = nextHead;
        }
        else
        {
            uart1RxDroppedBytes++;
        }

        if (HAL_UART_Receive_IT(&huart1, &uart1RxByte, 1U) != HAL_OK)
        {
            uart1RxDroppedBytes++;
        }
    }
    else if ((huart != NULL) && (huart->Instance == USART2))
    {
        uint16_t nextHead = (uint16_t)((uart2RxHead + 1U) % UART2_RX_RING_SIZE);
        if (nextHead != uart2RxTail)
        {
            uart2RxRing[uart2RxHead] = uart2RxByte;
            uart2RxHead = nextHead;
        }
        else
        {
            uart2RxDroppedBytes++;
        }

        if (HAL_UART_Receive_IT(&huart2, &uart2RxByte, 1U) != HAL_OK)
        {
            uart2RxDroppedBytes++;
        }
    }
}

/*
 * Triple pression sur USER en moins de 1,5 s.
 * La callback ne fait que compter les appuis; le protocole est traite
 * ensuite dans CommunicationTask, hors interruption.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint32_t now;

    if (GPIO_Pin != USER_BUTTON_Pin)
    {
        return;
    }

    now = HAL_GetTick();
    if ((userButtonLastPressTick != 0U) &&
        ((now - userButtonLastPressTick) < 180U))
    {
        return; /* anti-rebond */
    }
    userButtonLastPressTick = now;

    if ((userButtonPressCount == 0U) ||
        ((now - userButtonFirstPressTick) > 1500U))
    {
        userButtonPressCount = 1U;
        userButtonFirstPressTick = now;
    }
    else
    {
        userButtonPressCount++;
    }

    if (userButtonPressCount >= 3U)
    {
        buttonResetRequested = true;
        userButtonPressCount = 0U;
        userButtonFirstPressTick = 0U;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((huart != NULL) && (huart->Instance == USART1))
    {
        /* Recuperer les erreurs HAL avant de relancer la reception. */
        (void)HAL_UART_AbortReceive(&huart1);
        (void)HAL_UART_Receive_IT(&huart1, &uart1RxByte, 1U);
    }
    else if ((huart != NULL) && (huart->Instance == USART2))
    {
        (void)HAL_UART_AbortReceive(&huart2);
        (void)HAL_UART_Receive_IT(&huart2, &uart2RxByte, 1U);
    }
}

/*
 * Genere une valeur pseudo-aleatoire de 32 bits pour les essais fonctionnels.
 * IMPORTANT : ce generateur n'est pas cryptographiquement securise.
 * Pour une vraie cle cryptographique, utiliser le RNG materiel s'il est
 * disponible et active dans la configuration STM32CubeMX.
 */
static uint32_t Protocol_GenerateKey32(void)
{
    static uint32_t state = 0x6D2B79F5U;

    state ^= HAL_GetTick();
    state ^= SysTick->VAL;
    if (state == 0U)
    {
        state = 0xA341316CU;
    }

    /* Xorshift32 */
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    if (state == 0U)
    {
        state = 0xA341316CU;
    }

    return state;
}

/*
 * Chiffrement pedagogique XOR avec reutilisation des 4 octets de la cle.
 * Attention : ce mecanisme n'est pas un chiffrement securise.
 * Appliquer deux fois la meme operation retrouve les octets d'origine.
 */
static void Protocol_XorCrypt(uint8_t *data, uint8_t length, uint32_t key)
{
    uint8_t keyBytes[4];
    uint8_t i;

    keyBytes[0] = (uint8_t)(key & 0xFFU);
    keyBytes[1] = (uint8_t)((key >> 8) & 0xFFU);
    keyBytes[2] = (uint8_t)((key >> 16) & 0xFFU);
    keyBytes[3] = (uint8_t)((key >> 24) & 0xFFU);

    for (i = 0U; i < length; i++)
    {
        data[i] ^= keyBytes[i % 4U];
    }
}

static void Console_ProcessLine(const char *line)
{
    RxQueueItem_t item = {0};
    char normalized[32];
    size_t start = 0U;
    size_t end;
    size_t length;
    size_t i;

    if (line == NULL)
    {
        return;
    }

    /* Retirer uniquement les espaces en debut/fin; conserver la casse du message. */
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
    length = end - start;

    if (length == 0U)
    {
        return;
    }

    /* Copie en majuscules uniquement pour reconnaitre I/INIT. */
    for (i = 0U; (i < length) && (i < (sizeof(normalized) - 1U)); i++)
    {
        char c = line[start + i];
        if ((c >= 'a') && (c <= 'z'))
        {
            c = (char)(c - 'a' + 'A');
        }
        normalized[i] = c;
    }
    normalized[i] = '\0';

    /*
     * GET_KEY affiche uniquement la cle deja generee pendant INIT.
     * Ne pas regenerer la cle ici : les deux cartes doivent conserver
     * la meme cle pour que le dechiffrement continue de fonctionner.
     */
    if (strcmp(normalized, "GET_KEY") == 0)
    {
        char keyText[16];

        if ((!moduleIsMaster) || (moduleState != MODULE_SYNCHRONIZED) ||
            (moduleKey == 0U))
        {
            Console_Write("ERREUR: aucune cle disponible; lancez INIT d'abord.\r\n");
            return;
        }

        (void)snprintf(keyText, sizeof(keyText), "0x%08lX\r\n",
                       (unsigned long)moduleKey);
        Console_Write(keyText);
        return;
    }

    Console_Write("\r\nCommande recue: ");
    Console_Write(normalized);
    Console_Write("\r\n");

    if (strcmp(normalized, "REINITIALISATION") == 0)
    {
        item.message.header.start_byte = ICD_START_BYTE;
        item.message.header.command_type = CMD_RESET;
        item.message.header.length = 1U;
        item.message.header.reserve = 0U;
        item.message.header.id_src = DEFAULT_ID;
        item.message.payload[0] = 0U;
        item.message.crc = 0U;
        item.source = RX_SOURCE_PC;

        if (osMessageQueuePut(rxQueue, &item, 0U, 0U) != osOK)
        {
            Console_Write("ERREUR: impossible de mettre REINITIALISATION en file.\r\n");
        }
        else
        {
            Console_Write("Commande REINITIALISATION ajoutee a la file.\r\n");
        }
        return;
    }

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
        return;
    }

    /* GET_INFO demande au maitre les informations de la carte esclave. */
    if (strcmp(normalized, "GET_INFO") == 0)
    {
        if ((!moduleIsMaster) || (moduleState != MODULE_SYNCHRONIZED))
        {
            Console_Write("ERREUR: GET_INFO necessite un maitre initialise et synchronise.\r\n");
            return;
        }

        item.message.header.start_byte = ICD_START_BYTE;
        item.message.header.command_type = CMD_GET_INFO;
        item.message.header.length = 0U;
        item.message.header.reserve = 0U;
        item.message.header.id_src = DEFAULT_ID;
        item.message.crc = 0U;
        item.source = RX_SOURCE_PC;

        if (osMessageQueuePut(rxQueue, &item, 0U, 0U) != osOK)
        {
            Console_Write("ERREUR: impossible de mettre GET_INFO en file\r\n");
        }
        else
        {
            Console_Write("Demande GET_INFO ajoutee a la file de traitement.\r\n");
        }
        return;
    }

    /* INFO est une commande locale : elle ne modifie pas le protocole ICD. */
    if (strcmp(normalized, "INFO") == 0)
    {
        char info[160];
        const char *role;
        const char *state;

        if (moduleState == MODULE_UNINITIALIZED)
        {
            role = "NON DEFINI";
            state = "NON INITIALISE";
        }
        else
        {
            role = moduleIsMaster ? "MAITRE" : "ESCLAVE";
            if (moduleState == MODULE_SYNCHRONIZING)
            {
                state = "SYNCHRONISATION EN COURS";
            }
            else
            {
                state = "SYNCHRONISE";
            }
        }

        (void)snprintf(info, sizeof(info),
                       "\r\n--- INFORMATIONS DU MODULE ---\r\n"
                       "Identifiant (DEFAULT_ID): %lu\r\n"
                       "Role: %s\r\n"
                       "Position: %u\r\n"
                       "Statut: %s\r\n"
                       "------------------------------\r\n",
                       (unsigned long)DEFAULT_ID, role,
                       (unsigned int)modulePosition, state);
        Console_Write(info);
        return;
    }

    /* Toute autre ligne est un message texte, limite a la taille du payload ICD. */
    if (length > MAX_PAYLOAD_SIZE)
    {
        Console_Write("ERREUR: message trop long (maximum 16 caracteres).\r\n");
        return;
    }

    if ((!moduleIsMaster) || (moduleState != MODULE_SYNCHRONIZED))
    {
        Console_Write("ERREUR: initialiser et synchroniser le maitre avant d'envoyer un message.\r\n");
        return;
    }

    item.message.header.start_byte = ICD_START_BYTE;
    item.message.header.command_type = CMD_MSG;
    item.message.header.length = (uint8_t)length;
    item.message.header.reserve = 0U;
    item.message.header.id_src = DEFAULT_ID;
    memcpy(item.message.payload, &line[start], length);

    /*
     * Le maitre chiffre le texte avant de le placer dans la file.
     * Le test de cle non nulle evite d'envoyer un message chiffre avec une
     * cle qui n'a pas encore ete distribuee.
     */
    if (moduleKey == 0U)
    {
        Console_Write("ERREUR: aucune cle disponible. Lancez GET_KEY avant d'envoyer un message.\r\n");
        return;
    }
    Protocol_XorCrypt(item.message.payload, (uint8_t)length, moduleKey);

    item.message.crc = 0U;
    item.source = RX_SOURCE_PC;

    if (osMessageQueuePut(rxQueue, &item, 0U, 0U) != osOK)
    {
        Console_Write("ERREUR: file de traitement pleine, message non envoye.\r\n");
    }
    else
    {
        Console_Write("Message ajoute a la file de traitement.\r\n");
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
    if ((message->header.command_type != CMD_INIT)          &&
        (message->header.command_type != CMD_RESET)         &&
        (message->header.command_type != CMD_ACK)           &&
        (message->header.command_type != CMD_MSG)           &&
        (message->header.command_type != CMD_ERROR)         &&
        (message->header.command_type != CMD_GET_INFO)      &&
        (message->header.command_type != CMD_INFO_RESPONSE)  &&
        (message->header.command_type != CMD_GET_KEY)        &&
        (message->header.command_type != CMD_UPDT_KEY))
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
    else if ((message->header.command_type == CMD_ACK) ||
             (message->header.command_type == CMD_GET_INFO) ||
             (message->header.command_type == CMD_GET_KEY))
    {
        if (message->header.length != 0U)
        {
            return false;
        }
    }
    else if (message->header.command_type == CMD_RESET)
    {
        if ((message->header.length != 1U) || (message->payload[0] > 1U))
        {
            return false;
        }
    }
    else if (message->header.command_type == CMD_UPDT_KEY)
    {
        if (message->header.length != 4U)
        {
            return false;
        }
    }
    else if (message->header.command_type == CMD_INFO_RESPONSE)
    {
        /* id (4 octets) + position (1 octet) + statut (1 octet). */
        if (message->header.length != 6U)
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
            /* ACK de RESET: effacer l'etat local puis relancer INIT automatiquement. */
            if (moduleIsMaster && resetPending)
            {
                RxQueueItem_t initItem = {0};

                resetPending = false;
                keyUpdatePending = false;
                moduleKey = 0U;
                moduleState = MODULE_UNINITIALIZED;
                moduleIsMaster = false;
                modulePosition = 0U;
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
                Console_Write("ACK RESET recu: etat local efface. Relance automatique de INIT.\r\n");

                initItem.message.header.start_byte = ICD_START_BYTE;
                initItem.message.header.command_type = CMD_INIT;
                initItem.message.header.length = 1U;
                initItem.message.header.reserve = 0U;
                initItem.message.header.id_src = DEFAULT_ID;
                initItem.message.payload[0] = 0U;
                initItem.message.crc = 0U;
                initItem.source = RX_SOURCE_PC;

                if (osMessageQueuePut(rxQueue, &initItem, 0U, 0U) != osOK)
                {
                    Console_Write("ERREUR: RESET termine, mais INIT automatique n'a pas pu etre mis en file.\r\n");
                }
            }
            /* Priorite a l'ACK attendu pour UPDT_KEY. */
            else if (moduleIsMaster && keyUpdatePending)
            {
                keyUpdatePending = false;
                Console_Write("ACK recu: cle mise a jour sur la carte esclave.\r\n");
            }
            else if (moduleIsMaster && (moduleState == MODULE_SYNCHRONIZING))
            {
                uint8_t payload[4];
                char keyText[96];

                moduleState = MODULE_SYNCHRONIZED;
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
                Console_Write("ACK recu: chaine synchronisee (test 2 cartes)\r\n");

                /* Apres l'ACK d'INIT, generation et distribution automatiques de la cle. */
                moduleKey = Protocol_GenerateKey32();
                payload[0] = (uint8_t)(moduleKey & 0xFFU);
                payload[1] = (uint8_t)((moduleKey >> 8) & 0xFFU);
                payload[2] = (uint8_t)((moduleKey >> 16) & 0xFFU);
                payload[3] = (uint8_t)((moduleKey >> 24) & 0xFFU);

                (void)snprintf(keyText, sizeof(keyText),
                               "INIT termine: cle 32 bits generee: 0x%08lX\r\n",
                               (unsigned long)moduleKey);
                Console_Write(keyText);

                keyUpdatePending = true;
                Protocol_SendUart1(CMD_UPDT_KEY, payload, 4U);
                Console_Write("Cle envoyee automatiquement a la carte esclave; attente ACK.\r\n");
            }
            else
            {
                Console_Write("ACK recu\r\n");
            }
            break;

        case CMD_GET_INFO:
        {
            if (item->source == RX_SOURCE_PC)
            {
                if (moduleIsMaster && (moduleState == MODULE_SYNCHRONIZED))
                {
                    Protocol_SendUart1(CMD_GET_INFO, NULL, 0U);
                    Console_Write("Demande GET_INFO transmise a la carte esclave.\r\n");
                }
                else
                {
                    Console_Write("GET_INFO refuse: seul le maitre synchronise peut envoyer cette demande.\r\n");
                }
            }
            else
            {
                /* ID encode explicitement en little-endian, puis position et statut. */
                uint8_t payload[6];
                uint32_t id = DEFAULT_ID;

                payload[0] = (uint8_t)(id & 0xFFU);
                payload[1] = (uint8_t)((id >> 8) & 0xFFU);
                payload[2] = (uint8_t)((id >> 16) & 0xFFU);
                payload[3] = (uint8_t)((id >> 24) & 0xFFU);
                payload[4] = modulePosition;
                payload[5] = (uint8_t)moduleState;

                Protocol_SendUart1(CMD_INFO_RESPONSE, payload, 6U);
                Console_Write("Reponse GET_INFO envoyee au maitre.\r\n");
            }
            break;
        }

        case CMD_UPDT_KEY:
        {
            if ((item->source == RX_SOURCE_USART1) &&
                (!moduleIsMaster) &&
                (message->header.length == 4U))
            {
                moduleKey = ((uint32_t)message->payload[0]) |
                            ((uint32_t)message->payload[1] << 8) |
                            ((uint32_t)message->payload[2] << 16) |
                            ((uint32_t)message->payload[3] << 24);

                {
                    char keyText[96];
                    (void)snprintf(keyText, sizeof(keyText),
                                   "Cle recue et stockee sur l'esclave: 0x%08lX\r\n",
                                   (unsigned long)moduleKey);
                    Console_Write(keyText);
                }

                Protocol_SendUart1(CMD_ACK, NULL, 0U);
                Console_Write("ACK renvoye apres mise a jour de la cle.\r\n");
            }
            else
            {
                Console_Write("UPDT_KEY refuse: source ou longueur invalide.\r\n");
            }
            break;
        }

        case CMD_INFO_RESPONSE:
        {
            if ((item->source == RX_SOURCE_USART1) &&
                moduleIsMaster && (moduleState == MODULE_SYNCHRONIZED))
            {
                uint32_t id;
                uint8_t position = message->payload[4];
                uint8_t status = message->payload[5];
                const char *statusText;

                id = ((uint32_t)message->payload[0]) |
                     ((uint32_t)message->payload[1] << 8) |
                     ((uint32_t)message->payload[2] << 16) |
                     ((uint32_t)message->payload[3] << 24);

                switch (status)
                {
                    case MODULE_UNINITIALIZED:
                        statusText = "NON INITIALISE";
                        break;
                    case MODULE_SYNCHRONIZING:
                        statusText = "SYNCHRONISATION EN COURS";
                        break;
                    case MODULE_SYNCHRONIZED:
                        statusText = "SYNCHRONISE";
                        break;
                    default:
                        statusText = "INCONNU";
                        break;
                }

                {
                    char info[160];
                    (void)snprintf(info, sizeof(info),
                                   "\r\n--- INFORMATIONS DE L'ESCLAVE ---\r\n"
                                   "Identifiant: %lu\r\n"
                                   "Position: %u\r\n"
                                   "Statut: %s\r\n"
                                   "----------------------------------\r\n",
                                   (unsigned long)id,
                                   (unsigned int)position,
                                   statusText);
                    Console_Write(info);
                }
            }
            else
            {
                Console_Write("Reponse INFO recue hors contexte, ignoree.\r\n");
            }
            break;
        }

        case CMD_RESET:
        {
            uint8_t resetMode = message->payload[0];

            if (item->source == RX_SOURCE_PC)
            {
                if (moduleIsMaster)
                {
                    if (resetPending)
                    {
                        Console_Write("REINITIALISATION deja en cours.\r\n");
                        break;
                    }

                    resetPending = true;
                    resetMode = 0U;
                    Protocol_SendUart1(CMD_RESET, &resetMode, 1U);
                    Console_Write("REINITIALISATION envoyee a l'esclave; attente ACK.\r\n");
                }
                else
                {
                    resetMode = 1U;
                    Protocol_SendUart1(CMD_RESET, &resetMode, 1U);
                    Console_Write("Demande de reinitialisation envoyee au maitre.\r\n");
                }
            }
            else if (moduleIsMaster && (resetMode == 1U))
            {
                if (!resetPending)
                {
                    resetPending = true;
                    resetMode = 0U;
                    Protocol_SendUart1(CMD_RESET, &resetMode, 1U);
                    Console_Write("Demande RESET de l'esclave: reinitialisation coordonnee lancee.\r\n");
                }
            }
            else if ((!moduleIsMaster) && (resetMode == 0U))
            {
                moduleKey = 0U;
                keyUpdatePending = false;
                moduleState = MODULE_UNINITIALIZED;
                moduleIsMaster = false;
                modulePosition = 0U;
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
                Console_Write("RESET recu: cle et etat local effaces.\r\n");
                Protocol_SendUart1(CMD_ACK, NULL, 0U);
                Console_Write("ACK RESET envoye au maitre.\r\n");
            }
            else
            {
                Console_Write("RESET ignore: role ou mode invalide.\r\n");
            }
            break;
        }

        case CMD_MSG:
        {
            if (item->source == RX_SOURCE_PC)
            {
                if (moduleIsMaster && (moduleState == MODULE_SYNCHRONIZED))
                {
                    Protocol_SendUart1(CMD_MSG, message->payload,
                                       message->header.length);
                    Console_Write("Message transmis a la carte voisine.\r\n");
                }
                else
                {
                    Console_Write("ERREUR: transmission refusee, maitre non synchronise.\r\n");
                }
            }
            else
            {
                char receivedText[MAX_PAYLOAD_SIZE + 1U];
                uint8_t textLength = message->header.length;
                uint8_t decryptedPayload[MAX_PAYLOAD_SIZE];

                if (moduleKey == 0U)
                {
                    Console_Write("ERREUR: message chiffre recu mais aucune cle disponible.\r\n");
                    break;
                }

                memcpy(decryptedPayload, message->payload, textLength);
                Protocol_XorCrypt(decryptedPayload, textLength, moduleKey);
                memcpy(receivedText, decryptedPayload, textLength);
                receivedText[textLength] = '\0';

                Console_Write("Message dechiffre recu: ");
                Console_Write(receivedText);
                Console_Write("\r\n");
            }
            break;
        }

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
            if (item.source == RX_SOURCE_USART1)
            {
                Console_Write("\r\nTrame complete recue sur USART1 : ");
                for (size_t i = 0U; i < sizeof(ProtocolMessage_t); i++)
                {
                    Console_WriteHexByte(((const uint8_t *)&item.message)[i]);
                }
                Console_Write("\r\n");
            }

            if (Protocol_IsMessageValid(&item.message))
            {
                Protocol_ProcessMessage(&item);
            }
            else
            {
                Console_Write("ERREUR TRAME: commande invalide ou champs incoherents.\r\n");
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
