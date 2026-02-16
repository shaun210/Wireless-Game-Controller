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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include "string.h"
#include <stdint.h>
#include "hci_const.h"

#include "bluenrg_types.h"
#include "bluenrg_def.h"

/* 2. Stack Layers */
#include "hci.h"
#include "bluenrg_hal_aci.h"
#include "bluenrg_gap_aci.h"
#include "bluenrg_gatt_aci.h"


/* 3. Utils */
#include "hci_const.h"
#include "bluenrg_utils.h"

/* 4. Accelerometer */

#include "stm32l475e_iot01.h"
#include "stm32l475e_iot01_accelero.h"

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
I2C_HandleTypeDef hi2c2;
DMA_HandleTypeDef hdma_i2c2_rx;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* Global Handles */
uint16_t MyService_Handle;
uint16_t MyData_Char_Handle;

uint16_t gap_service_handle;
uint16_t dev_name_char_handle;
uint16_t appearance_char_handle;
uint8_t  counter = 0;   // Data to send

/* Your Custom UUIDs (Randomly generated) */
#define MY_SERVICE_UUID  0x1234
#define MY_CHAR_UUID     0x5678
#define GAP_PERIPHERAL_ROLE 0x01


/* SpaceController */
char right_msg[] = "right\n";
char left_msg[]  = "left\n";
char stop_msg[]  = "stop\n";
char fire_msg[]  = "fire\n";

int16_t accel_data[3];
uint8_t raw_accel_data[6];
GPIO_PinState last_button_state = GPIO_PIN_SET;
uint32_t last_button_time = 0;

enum Command { NONE, LEFT, RIGHT };
enum Command last_sent_cmd = NONE;

volatile uint8_t flag_button_pressed = 0;
volatile uint8_t flag_update_movement = 0;
volatile uint8_t flag_accel_ready = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// LSM6DSL I2C Address (Default for STM32L475 IoT Node)
#define LSM6DSL_ADDR  (0x6A << 1) // Becomes 0xD4
#define LSM6DSL_OUT_X_L  0x28     // Start of output registers



int _write(int file, char *ptr, int len)
{
  (void)file;
  int DataIdx;

  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    ITM_SendChar(*ptr++);
  }
  return len;
}


void My_BLE_Init(void)
{
  uint8_t bdaddr[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}; // Your MAC Address
  uint16_t service_uuid = MY_SERVICE_UUID;
  uint16_t char_uuid = MY_CHAR_UUID;
  const char local_name[] = {0x09, 'M', 'y', 'A', 'p', 'p'};

  /* 1. Wake up the Hardware (SPI & Chip) */
  /* Note: Ensure MX_SPI3_Init() is called before this! */
  hci_init(NULL, NULL);
  hci_reset();
  HAL_Delay(100);

  /* 2. Configure Address & Mode */
  aci_hal_write_config_data(CONFIG_DATA_PUBADDR_OFFSET, CONFIG_DATA_PUBADDR_LEN, bdaddr);
  aci_gatt_init(); // good
  aci_gap_init_IDB05A1(GAP_PERIPHERAL_ROLE, 0, 0x07, &gap_service_handle, &dev_name_char_handle, &appearance_char_handle);

//  aci_gap_init(role, <- not sure what exact val, apart from that looks good
//                   0,
//                   SVCCTL_GAP_DEVICE_NAME_LENGTH,
//                   &gap_service_handle,
//                   &gap_dev_name_char_handle,
//                   &gap_appearance_char_handle);

  /* 3. Create the Service (The Container) */
  aci_gatt_add_serv(UUID_TYPE_16, // good
                    (const uint8_t *)&service_uuid, // good
                    PRIMARY_SERVICE, // same
                    7,
                    &MyService_Handle); // good

  /* 4. Create the Characteristic (The Data Slot) */
  aci_gatt_add_char(MyService_Handle,
                    UUID_TYPE_16,
                    (const uint8_t *)&char_uuid,
                    1,  /* Max Data Length (1 Byte) */ // they used 2
                    CHAR_PROP_NOTIFY | CHAR_PROP_READ, // CHAR_PROP_WRITE_WITHOUT_RESP|CHAR_PROP_READ,
                    ATTR_PERMISSION_NONE, // security meaning anyone can read data
                    GATT_NOTIFY_ATTRIBUTE_WRITE, // same
                    16,
                    0,
                    &MyData_Char_Handle); /* <--- SAVE THIS HANDLE! */

  /* 5. Start Advertising ("Turns on the radio" and starts shouting "I am here!" (Advertising).) */
  tBleStatus ret = aci_gap_set_discoverable(ADV_IND, 0, 0, PUBLIC_ADDR, NO_WHITE_LIST_USE,
                           sizeof(local_name), local_name, 0, NULL, 0, 0);


//  if( ret == BLE_STATUS_SUCCESS )
//      {
//	  printf("success");
//
//      }
//  else {
//	  printf("failed");
//
//      }
}


//
//void space_invader_controller_non_blocking(void) {
//    static uint32_t last_run_time = 0;
//    static uint8_t last_move_val = 255;
//
//    // 1. Process Button (Highest Priority)
////     If the Interrupt set the flag, we handle it now
//    if (flag_button_pressed) {
//        flag_button_pressed = 0; // Clear flag
//
//        uint8_t fire_val = 3;
//        aci_gatt_update_char_value(MyService_Handle, MyData_Char_Handle, 0, 1, &fire_val);
//
//        // Force movement update next cycle so we don't get stuck
//        last_move_val = 255;
//    }
//
//    // 2. Check Time for Movement Update (e.g., every 50ms)
//    if ((HAL_GetTick() - last_run_time) >= 50) {
//        last_run_time = HAL_GetTick(); // Reset timer
//
//        // --- Read Accelerometer ---
//        int16_t accel_data[3];
//        BSP_ACCELERO_AccGetXYZ(accel_data);
//
//
//        uint8_t current_move_val = 0;
//        if (accel_data[0] > 500) current_move_val = 2;
//        else if (accel_data[0] < -500) current_move_val = 1;
//
//        // --- Send BLE Update if changed ---
//        if (current_move_val != last_move_val) {
//            tBleStatus result = aci_gatt_update_char_value(MyService_Handle, MyData_Char_Handle, 0, 1, &current_move_val);
//            if (result == BLE_STATUS_SUCCESS) {
//                last_move_val = current_move_val;
//            }
//        }
//    }
//}

void space_invader_controller_non_blocking(void) {
    static uint32_t last_run_time = 0;
    static uint8_t last_move_val = 255;

    // 1. Process Button
    if (flag_button_pressed) {
        flag_button_pressed = 0;
        uint8_t fire_val = 3;
        aci_gatt_update_char_value(MyService_Handle, MyData_Char_Handle, 0, 1, &fire_val);
        last_move_val = 255;
    }

    // 2. Trigger DMA Read every 50ms
    if ((HAL_GetTick() - last_run_time) >= 50) {
        last_run_time = HAL_GetTick();

        // Start DMA transfer: Read 6 bytes (X, Y, Z low/high regs) starting from OUT_X_L
        // This returns immediately. The CPU continues to while(1)
        HAL_I2C_Mem_Read_DMA(&hi2c2, LSM6DSL_ADDR, LSM6DSL_OUT_X_L, I2C_MEMADD_SIZE_8BIT, raw_accel_data, 6);
    }

    // 3. Check if DMA is finished
    if (flag_accel_ready) {
        flag_accel_ready = 0; // Clear flag

        // Convert raw bytes to int16_t (Little Endian)
        accel_data[0] = (int16_t)((raw_accel_data[1] << 8) | raw_accel_data[0]);
        // (You can do Y and Z here too if needed)

        uint8_t current_move_val = 0;
        if (accel_data[0] > 500) current_move_val = 2;
        else if (accel_data[0] < -500) current_move_val = 1;

        if (current_move_val != last_move_val) {
            tBleStatus result = aci_gatt_update_char_value(MyService_Handle, MyData_Char_Handle, 0, 1, &current_move_val);
            if (result == BLE_STATUS_SUCCESS) {
                last_move_val = current_move_val;
            }
        }
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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  BSP_ACCELERO_Init();
  My_BLE_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  printf("starting the program \r\n");
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  // 1. Keep the Bluetooth Stack alive (REQUIRED)
	hci_user_evt_proc();

	// 2. Run your game logic
	space_invader_controller_non_blocking();



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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10D19CE4;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
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
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

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
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(myLed_GPIO_Port, myLed_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BLE_CS_GPIO_Port, BLE_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BLE_RST_GPIO_Port, BLE_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BLE_IRQ_Pin */
  GPIO_InitStruct.Pin = BLE_IRQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BLE_IRQ_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : myButton_Pin */
  GPIO_InitStruct.Pin = myButton_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(myButton_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : myLed_Pin */
  GPIO_InitStruct.Pin = myLed_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(myLed_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BLE_CS_Pin */
  GPIO_InitStruct.Pin = BLE_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BLE_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BLE_RST_Pin */
  GPIO_InitStruct.Pin = BLE_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BLE_RST_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == myButton_Pin) {
        // Simple software debounce check
//        static uint32_t last_irq_time = 0;
//        uint32_t current_time = HAL_GetTick();
//
//        if ((current_time - last_irq_time) > 200) {
//            flag_button_pressed = 1; // Tell main loop to fire!
//            last_irq_time = current_time;
//        }
        flag_button_pressed = 1; // Tell main loop to fire!
    }
}


void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c2)
{
    // Check if it's the right I2C bus (in case you have others)

    if (hi2c2->Instance == I2C2) {
        flag_accel_ready = 1; // Tell the main loop: "Data is here!"
    }
}


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
