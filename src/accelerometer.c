// accelerometer.c
#include <driver/i2c.h>
#include <esp_log.h>

#define MPU6050_REG_CHIP_ID             0x75
#define MPU6050_CHIP_ID                 0x68

#define MPU6050_REG_GYRO_CFG            0x1B
#define MPU6050_GYRO_FS_SHIFT           3

#define MPU6050_REG_ACCEL_CFG           0x1C
#define MPU6050_ACCEL_FS_SHIFT          3

#define MPU6050_REG_INT_EN              0x38
#define MPU6050_DRDY_EN                 BIT(0)

#define MPU6050_REG_DATA_START          0x3B

#define MPU6050_REG_PWR_MGMT1           0x6B
#define MPU6050_SLEEP_EN                BIT(6)

#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

#define MPU6050_ADDR 0x68

#define tag "MPU6050"


static esp_err_t i2c_read(uint8_t reg, uint8_t *data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}


static esp_err_t i2c_write_byte(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

int i2c_read_byte(int reg) {
	uint8_t val;
	i2c_read(reg, &val, 1);
	return val;
}

int16_t bufmem[7];

void read_mpu6050(int16_t buf[7]) {
	i2c_read(MPU6050_REG_DATA_START, (uint8_t *)&bufmem, 14);
	for(int i=0;i<7;i++) {
		buf[i]=(bufmem[i]<<8) | (bufmem[i]>>8 & 0xff);
	}
	ESP_LOGI(tag, "read %d %d %d %d %d %d %d\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);
}

// GPIOs to use

#define VCC 32
#define GND 33
#define SCL 25
#define SDA 26


void mpu6050_init() {
    gpio_set_direction(GND,GPIO_MODE_OUTPUT);
    gpio_set_level(GND,0);
    gpio_set_direction(VCC,GPIO_MODE_OUTPUT);
    gpio_set_level(VCC,1);
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    ESP_LOGI(tag, "sda_io_num %d", SDA);
    conf.sda_io_num = SDA;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    ESP_LOGI(tag, "scl_io_num %d", SCL);
    conf.scl_io_num = SCL;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    ESP_LOGI(tag, "clk_speed %d", 50000);
    conf.master.clk_speed = 50000;
    ESP_LOGI(tag, "i2c_param_config %d", conf.mode);
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_1, &conf));
    ESP_LOGI(tag, "i2c_driver_install %d", I2C_NUM_1);
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_1, conf.mode, 0, 0, 0));

	int id=i2c_read_byte(MPU6050_REG_CHIP_ID);
	
	ESP_LOGI(tag, "Found MPU6050 id=%x\n", id);
	
	int pwr=i2c_read_byte(MPU6050_REG_PWR_MGMT1);
	ESP_LOGI(tag, "pwr=%x\n", pwr);
	pwr &= ~(MPU6050_SLEEP_EN);
	
	i2c_write_byte(MPU6050_REG_PWR_MGMT1,pwr);
	
	i2c_write_byte(MPU6050_REG_ACCEL_CFG, 0); // 2g fs
	i2c_write_byte(MPU6050_REG_GYRO_CFG, 0); //  250DEG/SEC 
}
