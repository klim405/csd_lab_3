/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ring_buffer.h"
#include "sounds.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define TX6_BUFFER_SIZE 100
#define DEFAULT_BUZZER_VOLUME 10
#define CUSTOM_MUSIC_LENGTH 50
#define NOTE_TIC_DURATION_IN_MILIS 100
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
char __uart6_tx_buff[TX6_BUFFER_SIZE];
RingBuffer uart6_tx_buff;
bool UART6_TX_IsReady = true;
char uart6_tx_byte_buff[1];

bool UART6_RX_IsReady = false;
char uart6_rx_byte_buff[1];

uint32_t Buzzer_Volume = DEFAULT_BUZZER_VOLUME;

char CustomMusic[CUSTOM_MUSIC_LENGTH * 3 + 1] = { '\0' };
char* MusicList[5] = {
  "C45A45A45G45A45F45C45C45C45A45A45a45G45C55C55D45D45a45a45A45G45F45C45A45A45G45A45F45", 
  "C79D73E73F73G73A73H73C89D83E83F83G83A83H83C99D93E93F93G93A93H93",
  "",
  "", 
  CustomMusic
};

struct __MusicPlayer {
  volatile char * next_sound;
  uint16_t current_sound_duration;
  uint16_t current_sound_playing_time;
  bool is_playing;
} typedef MusicPlayer;

MusicPlayer player = { 0, 0, 0, false };

enum __UserInputState {
  WAIT_MUSIC_NUMBER = 0,
  WAIT_NOTE,
  WAIT_OCTAVE,
  WAIT_DURATION
} typedef UserInputState;

struct __UserInput {
  UserInputState status;
  char* current_sound;
  int32_t current_sound_number;
} typedef UserInputTypedef;

UserInputTypedef UserInput;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief Пытается отправить данные из кольцевого буфера
 */
void UART6_TryToTransmit_IT() {
  if (UART6_TX_IsReady && !RingBuffer_IsEmpty(&uart6_tx_buff)) {
    RingBuffer_Read(&uart6_tx_buff, uart6_tx_byte_buff, 1);
    UART6_TX_IsReady = false;
    HAL_UART_Transmit_IT(&huart6, (uint8_t*) uart6_tx_byte_buff, 1);
  } 
}

/**
 * @brief Отправляет байт в режиме прерываний
 * @return true - байт записан в буфер отправки
 * @return false - буфер отправки переполнен
 */
bool UART6_TransmitByte(char byte) {
  bool result = false;
  if (!uart6_tx_buff.isFull) {
      RingBuffer_Write(&uart6_tx_buff, &byte, 1);
      result = true;
    }
  return result;
}

/**
 * @brief Получает байт по uart
 * @return true - данные получены
 * @return false - новых данных нет
 */
bool UART6_ReceiveByte(char* byte_ptr) {
  bool result = false;
  if (UART6_RX_IsReady) {
      *byte_ptr = *uart6_rx_byte_buff;
      UART6_RX_IsReady = false;
      result = true;
  } else {
    HAL_UART_Receive_IT(&huart6, (uint8_t*) uart6_rx_byte_buff, 1);
  }
  return result;
}

/**
 * @brief Получает байт по uart
 * @return true - данные записан в буфер отправки
 * @return false - буфер отправки переполнен
 */
bool UART6_TransmitString(char* str) {
  bool result = false;
  if (!uart6_tx_buff.isFull) {
    RingBuffer_Write(&uart6_tx_buff, str, strlen(str));
  }
  return result;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* UartHandle) {
  if (UartHandle == &huart6) {
    UART6_RX_IsReady = true;
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* UartHandle) {
  if (UartHandle == &huart6) {
    UART6_TX_IsReady = true;
  }
}

/**
 * @brief Воспроизводит заданную ноту на звукоизлучателе
 * @param octave номер октавы
 * @param note номер ноты в октаве
 */
void Buzzer_SetSound(int8_t octave, int8_t note) {
  htim1.Instance->ARR = (uint32_t) Sounds_CalcPeriodInNs(octave, note);
  htim1.Instance->CNT = 0;
  htim1.Instance->CCR1 = (htim1.Instance->ARR / 20) * Buzzer_Volume;
}

/**
 * @brief Воспроизводит заданную ноту на звукоизлучателе
 * @param val громкость от 0 до 10
 */
void Buzzer_SetVolume(uint32_t val) {
  if (val > 10) {
    val = 10;
  }
  Buzzer_Volume = val;
  htim1.Instance->CCR1 = (htim1.Instance->ARR / 20) * Buzzer_Volume;
}

/**
 * @brief Отключает динамик.
 * @note Таймер динамика не отключается. Заполнение ШИМ ~0%
 */
void Buzzer_Stop() {
  htim1.Instance->CCR1 = 1;
}

/**
 * @brief Останавливает проигрывание музыки.
 */
void MusicPlayer_Stop() {
  player.is_playing = false;
  Buzzer_Stop();
}

/**
 * @brief Запускает следующий звук, если он есть.
 */
void MusicPlayer_PlayNextSound() {
  int8_t note;
  int8_t octave;
  if (*player.next_sound != '\0') {
	  UART6_TransmitByte(player.next_sound[0]);
	  UART6_TransmitByte(player.next_sound[1]);
	  UART6_TransmitByte(player.next_sound[2]);
	  UART6_TransmitByte('\n');
    note = Sounds_ParseNote(player.next_sound[0]);
    octave = (player.next_sound[1] - '0');
    player.current_sound_duration = (player.next_sound[2] - '0') * NOTE_TIC_DURATION_IN_MILIS;
    player.current_sound_playing_time = 0;
    player.next_sound = player.next_sound + 3;
    Buzzer_SetSound(octave, note);
  } else {
    MusicPlayer_Stop();
  }
}

/**
 * @brief Запускает музыку из памяти.
 * @param number Номер музыкальной дорожки.
 */
void MusicPlayer_RunMusic(int8_t number) {
  player.next_sound = MusicList[number - 1];
  player.is_playing = true;
  MusicPlayer_PlayNextSound();
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM6) {
    if (player.is_playing) {
      if (player.current_sound_playing_time < player.current_sound_duration) {
        ++player.current_sound_playing_time;
      } else {
        MusicPlayer_PlayNextSound();
      }
    }
  }
}

void UserInput_ProcessInput(char sym) {
  switch (UserInput.status) {
  case WAIT_MUSIC_NUMBER:
  UART6_TransmitByte(sym);
    if ('0' < sym && sym <= '5') {
      UART6_TransmitString("\nPlay music ");
      UART6_TransmitByte(sym);
      UART6_TransmitByte('\n');
      MusicPlayer_RunMusic(sym - '0');
    } else if (sym == '\n' || sym == '\r') {
      MusicPlayer_Stop();
      UserInput.status = WAIT_NOTE;
      UserInput.current_sound = CustomMusic;
      UserInput.current_sound_number = 0;
    }
    break;
  case WAIT_NOTE:
    if (UserInput.current_sound_number >= CUSTOM_MUSIC_LENGTH) {
      UART6_ReceiveByte("Song register is full. Music was saved.\nEnter music number:\n");
      UserInput.status = WAIT_MUSIC_NUMBER;
      break;
    }
    if (Sounds_ParseNote(sym) >= 0) {
      UserInput.current_sound[0] = sym;
      UART6_ReceiveByte(sym);
      UserInput.status = WAIT_OCTAVE;
    } else if (sym == 'q' || sym == 's') {
      UserInput.current_sound[0] = '\0';
      UART6_ReceiveByte("Music was saved.\nEnter music number:\n");
      UserInput.status = WAIT_MUSIC_NUMBER;
    }
    break;
  case WAIT_OCTAVE:
    if ('0' <= sym <= '9') {
      UserInput.current_sound[1] = sym;
      UART6_ReceiveByte(sym);
      UserInput.status = WAIT_DURATION;
    }
    break;
  case WAIT_DURATION:
    if ('1' <= sym <= '9') {
      UserInput.current_sound[2] = sym;
      UART6_ReceiveByte(sym);
      UserInput.current_sound = UserInput.current_sound + 3;
      ++UserInput.current_sound_number;
      UserInput.status = WAIT_NOTE;
    }
    break;
  }
}

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
  RingBuffer_Init(&uart6_tx_buff, __uart6_tx_buff, TX6_BUFFER_SIZE);
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_TIM6_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim6);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  char cuurent_sym;
  while (1)
  {
    UART6_TryToTransmit_IT();
    if (UART6_ReceiveByte(&cuurent_sym)) {
      UserInput_ProcessInput(cuurent_sym);
    }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 15;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

#ifdef  USE_FULL_ASSERT
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
