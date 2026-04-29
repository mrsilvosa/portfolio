/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define EXTBUTTON_Pin GPIO_PIN_13
#define EXTBUTTON_GPIO_Port GPIOC
#define INTENSIDAD_ADC_IN_Pin GPIO_PIN_2
#define INTENSIDAD_ADC_IN_GPIO_Port GPIOC
#define TFT_SCK_Pin GPIO_PIN_5
#define TFT_SCK_GPIO_Port GPIOA
#define TFT_MISO_Pin GPIO_PIN_6
#define TFT_MISO_GPIO_Port GPIOA
#define TFT_MOSI_Pin GPIO_PIN_7
#define TFT_MOSI_GPIO_Port GPIOA
#define SD_SPI_CS_Pin GPIO_PIN_5
#define SD_SPI_CS_GPIO_Port GPIOC
#define TFT_DC_Pin GPIO_PIN_1
#define TFT_DC_GPIO_Port GPIOB
#define TFT_RESET_Pin GPIO_PIN_2
#define TFT_RESET_GPIO_Port GPIOB
#define TFT_SPI_CS_Pin GPIO_PIN_12
#define TFT_SPI_CS_GPIO_Port GPIOB
#define SD_SCK_Pin GPIO_PIN_13
#define SD_SCK_GPIO_Port GPIOB
#define SD_MISO_Pin GPIO_PIN_14
#define SD_MISO_GPIO_Port GPIOB
#define SD_MOSI_Pin GPIO_PIN_15
#define SD_MOSI_GPIO_Port GPIOB
#define BOTON_DOWN_Pin GPIO_PIN_8
#define BOTON_DOWN_GPIO_Port GPIOC
#define B1_Pin GPIO_PIN_9
#define B1_GPIO_Port GPIOC
#define ESPECTRAL_SCL_Pin GPIO_PIN_8
#define ESPECTRAL_SCL_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define BOTON_LEFT_Pin GPIO_PIN_11
#define BOTON_LEFT_GPIO_Port GPIOC
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB
#define ESPECTRAL_SDA_Pin GPIO_PIN_4
#define ESPECTRAL_SDA_GPIO_Port GPIOB
#define TFT_LED_Pin GPIO_PIN_5
#define TFT_LED_GPIO_Port GPIOB
#define BOTON_UP_Pin GPIO_PIN_6
#define BOTON_UP_GPIO_Port GPIOB
#define BOTON_RIGHT_Pin GPIO_PIN_9
#define BOTON_RIGHT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

// redefiniciones para el funcionamiento de la SD:
//#define SD_HSPIX hspi2
//#define SD_GPIOX SD_SPI_CS_GPIO_Port
//#define SD_GPIO_PIN_X SD_SPI_CS_Pin

// redefiniciones para el funcionamiento del sensor espectro:
//#define AS726X_HI2CX hi2c3

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
