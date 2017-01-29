config set defined in 2016-02-28

  arduino requests configuration with 'config?' on reset
  
  configuration commands start with 'c'
   cdebug:<data> debug settings, data are hex (0,1,2,3)
   cr: dummy request, just get a newline and clean buffer
   cversion? request version string
   cerr?     request error count for parser
   cident?     request idcode
   cident:<char16> write idcode
  
   cdin:<data> digital inputs, data are hex
   cdinp:<data> digital inputs, pullup enabled, data are hex
   cdout:<data> digital outputs, data are hex
   cdpwm:<data> digital pwm, data are hex
   cdservo:<data> digital servo, data are hex

   caain:<data> analog line, data are hex
   cadin:<data> analog line, digital input
   cadinp:<data> analog line, digital input, pullup
   cadout:<data> analog line, digital output
  data give bit patterns for IO pins, Bits 1,2,3... are used
  
  Commands to set values in arduino
   o:<port>,<value>     write output, shortcut
   p:<port>,<value>     write pwm, shortcut
   s:<port>,<value>     write servo, shortcut
  
  Values reported from arduino to host
   v:<version>       arduino reports version
   ident:<char16>    arduino reports ident from EEPROM
   e:<errors>        arduino reports number of errors (decimal)
   a:<port>,<value>  arduino reports analog input
   i:<port>,<value>  arduino reports digital input
   ai:<port>,<value> arduino reports digital input;
