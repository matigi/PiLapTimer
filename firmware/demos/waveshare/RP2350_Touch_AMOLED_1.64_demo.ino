/*
  Reference Demo – DO NOT MODIFY

  Source: Waveshare RP2350-Touch-AMOLED-1.64 official demo
  Purpose:
  - Canonical AMOLED + QSPI + Touch initialization sequence
  - Framebuffer allocation reference
  - Touch controller init mode reference

  This file exists to prevent regressions in display bring-up.
  Production code lives in firmware/pilaptimer/.
*/


#include "DEV_Config.h"
#include "AMOLED_1in64.h"
#include "qspi_pio.h"
#include "QMI8658.h"
#include "FT3168.h"
#include "image.h"
#include "GUI_Paint.h"

unsigned char const *PIC;
int flag=0;
uint8_t i2c_lock = 0;
#define I2C_LOCK() i2c_lock = 1
#define I2C_UNLOCK() i2c_lock = 0

void Touch_INT_callback(uint gpio, uint32_t events);

void setup()
{
   if (DEV_Module_Init() != 0)
        Serial.println("GPIO Init Fail!");
    else
        Serial.println("GPIO Init successful!");
    /* LCD Init */
     /*QSPI PIO Init*/
    QSPI_GPIO_Init(qspi);
    QSPI_PIO_Init(qspi);
    QSPI_1Wrie_Mode(&qspi);

    /*AMOLED Init*/
    printf("1.64inch AMOLED demo...\r\n");
    AMOLED_1IN64_Init();
    AMOLED_1IN64_SetBrightness(100);
    AMOLED_1IN64_Clear(WHITE);
    
    UDOUBLE Imagesize = AMOLED_1IN64_HEIGHT*AMOLED_1IN64_WIDTH*2;
    UWORD *BlackImage;
    if((BlackImage = (UWORD *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        exit(0);
    }

    /*1.Create a new image cache named IMAGE_RGB and fill it with white*/
    Paint_NewImage((UBYTE *)BlackImage, AMOLED_1IN64.WIDTH, AMOLED_1IN64.HEIGHT, 0, WHITE);
    Paint_SetScale(65);
    Paint_SetRotate(ROTATE_0);
    Paint_Clear(WHITE);
    AMOLED_1IN64_Display(BlackImage);

    /* GUI */
    printf("drawing...\r\n");
    /*2.Drawing on the image*/
#if 1
    Paint_DrawPoint(17, 21, BLACK, DOT_PIXEL_2X2, DOT_FILL_RIGHTUP);
    Paint_DrawPoint(17, 26, BLACK, DOT_PIXEL_3X3, DOT_FILL_RIGHTUP); 
    Paint_DrawPoint(17, 31, BLACK, DOT_PIXEL_4X4, DOT_FILL_RIGHTUP); 
    Paint_DrawPoint(17, 36, BLACK, DOT_PIXEL_5X5, DOT_FILL_RIGHTUP); 
    Paint_DrawPoint(17, 42, BLACK, DOT_PIXEL_6X6, DOT_FILL_RIGHTUP);

    Paint_DrawLine(25, 20, 55, 50, MAGENTA, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(25, 50, 55, 20, MAGENTA, DOT_PIXEL_2X2, LINE_STYLE_SOLID);  

    Paint_DrawLine(95, 35, 125, 35, CYAN, DOT_PIXEL_1X1, LINE_STYLE_DOTTED); 
    Paint_DrawLine(110, 20, 110, 50, CYAN, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);  

    Paint_DrawRectangle(25, 20, 55, 50, RED, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);   
    Paint_DrawRectangle(60, 20, 90, 50, BLUE, DOT_PIXEL_2X2, DRAW_FILL_FULL); 

    Paint_DrawCircle(110, 35, 15, GREEN, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(145, 35, 15, GREEN, DOT_PIXEL_1X1, DRAW_FILL_FULL);  

    Paint_DrawNum (70, 55, 9.87654321, &Font24, 5, WHITE, BLACK);
    Paint_DrawString_EN(16, 55, "ABC", &Font24, 0x000f, 0xfff0);
    Paint_DrawString_CN(16, 75, "欢迎使用", &Font24CN, WHITE, BLUE);
    Paint_DrawString_EN(16, 115, "WaveShare", &Font24, RED, WHITE); 

    AMOLED_1IN64_Display(BlackImage);
    DEV_Delay_ms(1000);
#endif

#if 1
    /*3.Refresh the picture in RAM to LCD*/
    Paint_DrawImage(gImage_image1,0,0,AMOLED_1IN64.WIDTH,AMOLED_1IN64.HEIGHT);
    AMOLED_1IN64_Display(BlackImage);
    DEV_Delay_ms(100);
#endif

#if 1
    /*4.Display six-axis sensor data*/
    float acc[3], gyro[3];
    unsigned int tim_count = 0;
    const float conversion_factor = 3.3f / (1 << 12) * 3;
    QMI8658_init();
    FT3168_Init(FT3168_Gesture_Mode);
    struct repeating_timer timer;
    add_repeating_timer_ms(1, timer_callback, NULL, &timer);
    
    Paint_Clear(WHITE);
    Paint_DrawRectangle(0, 0, 280, 152, 0XF410, DOT_PIXEL_2X2, DRAW_FILL_FULL);
    Paint_DrawRectangle(0, 152, 280, 304, 0X4F30, DOT_PIXEL_2X2, DRAW_FILL_FULL);
    Paint_DrawRectangle(0, 304, 280, 456, 0XAD55, DOT_PIXEL_2X2, DRAW_FILL_FULL);
    
    Paint_DrawString_EN(5, 70, "Double Click Out", &Font24, BLACK, 0XF410);
    Paint_DrawString_EN(5, 168, "ACC_X = ", &Font24, BLACK, 0X4F30);
    Paint_DrawString_EN(5, 211, "ACC_Y = ", &Font24, BLACK, 0X4F30);
    Paint_DrawString_EN(5, 254, "ACC_Z = ", &Font24, BLACK, 0X4F30);
    Paint_DrawString_EN(5, 318, "GYR_X = ", &Font24, BLACK, 0XAD55);
    Paint_DrawString_EN(5, 361, "GYR_Y = ", &Font24, BLACK, 0XAD55);
    Paint_DrawString_EN(5, 404, "GYR_Z = ", &Font24, BLACK, 0XAD55);
    AMOLED_1IN64_Display(BlackImage);

    while (true)
    {
        while(i2c_lock);
        I2C_LOCK();
        QMI8658_read_xyz(acc, gyro, &tim_count);
        I2C_UNLOCK();
        printf("acc_x   = %4.3fmg , acc_y  = %4.3fmg , acc_z  = %4.3fmg\r\n", acc[0], acc[1], acc[2]);
        printf("gyro_x  = %4.3fdps, gyro_y = %4.3fdps, gyro_z = %4.3fdps\r\n", gyro[0], gyro[1], gyro[2]);
        printf("tim_count = %d\r\n", tim_count);
        
        Paint_DrawRectangle(140, 168, 280, 290, 0X4F30, DOT_PIXEL_2X2, DRAW_FILL_FULL);
        Paint_DrawRectangle(140, 318, 280, 456, 0XAD55, DOT_PIXEL_2X2, DRAW_FILL_FULL);
        Paint_DrawNum(140, 168, acc[0], &Font24, 2, BLACK , 0X4F30);
        Paint_DrawNum(140, 211, acc[1], &Font24, 2, BLACK , 0X4F30);
        Paint_DrawNum(140, 254, acc[2], &Font24, 2, BLACK, 0X4F30);
        Paint_DrawNum(140, 318, gyro[0], &Font24, 2, BLACK, 0XAD55);
        Paint_DrawNum(140, 361, gyro[1], &Font24, 2, BLACK, 0XAD55);
        Paint_DrawNum(140, 404, gyro[2], &Font24, 2, BLACK, 0XAD55);

        AMOLED_1IN64_Display(BlackImage);
        DEV_Delay_ms(100);
        if (flag == 1)
        {
            flag = 0;
            break;
        }
    }
#endif

#if 1
    /*5.Drawing test*/
    while(i2c_lock);
    I2C_LOCK();
    FT3168_Init(FT3168_Point_Mode);
    I2C_UNLOCK();
    Paint_Clear(WHITE);
    AMOLED_1IN64_Display(BlackImage);
    Paint_DrawRectangle(0, 0, 280, 60, 0X2595, DOT_PIXEL_2X2, DRAW_FILL_FULL);
    Paint_DrawString_EN(50, 30, "Touch test", &Font24, BLACK, 0X2595);
    AMOLED_1IN64_Display(BlackImage);
    while (true)
    {
        if (flag)
        {
            Paint_DrawPoint(FT3168.x_point, FT3168.y_point, RED, DOT_PIXEL_8X8, DOT_FILL_AROUND);
            AMOLED_1IN64_DisplayWindows(FT3168.x_point, FT3168.y_point, FT3168.x_point + 8, FT3168.y_point + 8, BlackImage);
            printf("X:%d Y:%d\r\n", FT3168.x_point, FT3168.y_point);
            flag = 0;
        }
        __asm__ volatile("nop");
    }
#endif

     /* Module Exit */
     free(BlackImage);
     BlackImage = NULL;

     DEV_Module_Exit();

}

void loop()
{
     DEV_Delay_ms(1000);
} 



bool timer_callback(struct repeating_timer *t) {
    if(i2c_lock) return true; 
    if(FT3168.mode != FT3168_Point_Mode)
    {
        uint8_t gesture = FT3168_Get_Gesture();
            
        if (gesture == FT3168_Gesture_Double_Click)
        {
            flag = 1;
        }
    }
    else
    {
        static int y_point_old,x_point_old;
        FT3168_Get_Point();
        if(FT3168.x_point != x_point_old || FT3168.y_point != y_point_old)
        {
            flag = 1;
        }
        y_point_old = FT3168.y_point;
        x_point_old = FT3168.x_point;
    }
    return true; 
}


