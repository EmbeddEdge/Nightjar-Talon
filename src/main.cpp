/*******************************************************************************
* Title                 :   Nightjar-Talon
* Filename              :   main.c
* Author                :   Turyn Lim Banda
* Origin Date           :   14/06/2021
* Version               :   1.0.0
* Compiler              :   PlatformIO
* Target                :   ESP32
* Notes                 :   None
*
* THIS SOFTWARE IS PROVIDED BY EMBEDDEDGE "AS IS" AND ANY EXPRESSED
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL EMBEDDEDGE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*
*******************************************************************************/
/*************** MODULE REVISION LOG ***************************************************************
*
*    Date    Version  Author           Description 
*  14/06/21   1.0.0   Turyn Lim Banda  Initial commit.
*
****************************************************************************************************/

/** @file main.c
 *  @brief The implementation for the Nightjar-Talon.
 */
/******************************************************************************
* Includes
*******************************************************************************/
#include <Arduino.h>
//#include "main.h"
/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/

/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/

/******************************************************************************
* Module Typedefs
*******************************************************************************/


/******************************************************************************
* Module Variable Definitions
*******************************************************************************/

void setup()
{
  delay(3000); //safety startup delay
  //Configure the Serial Output
  SerialMon.begin(115200);
  
}

void loop()
{
  //Do the initial checks i.e. is WiFi Connected? Is there serial data?
  #if DEBUG
  checkStatusWifi(); 
  #endif
  requestConfigPortal();  //Is configuration portal requested?        
  ReadSerialMon();        //Read the serial line if anything is there
  
}

