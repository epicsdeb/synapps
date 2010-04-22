
/* paramAttribute.h
 *
 * \author Mark Rivers
 *
 * \author University of Chicago
 *
 * \date April 30, 2009
 *
 */
#ifndef INCparamAttributeH
#define INCparamAttributeH

#include <ellLib.h>
#include <cadef.h>

#include "NDArray.h"
#include "asynNDArrayDriver.h"

/** Use native type for channel access */
#define DBR_NATIVE -1

typedef enum {
    paramAttrTypeInt,
    paramAttrTypeDouble,
    paramAttrTypeString,
    paramAttrTypeUnknown
} paramAttrType_t;

/** Attribute that gets its value from an asynNDArrayDriver driver parameter.
  * The updateValue() method for this class retrieves the current value of the driver parameter.
  */
class paramAttribute : public NDAttribute {
public:
    paramAttribute(const char *pName, const char *pDescription, const char *pSource, int addr, 
                    class asynNDArrayDriver *pDriver, const char *dataType);
    ~paramAttribute();
    virtual int updateValue();
    int report(int details);

private:
    int         paramId;
    int         paramAddr;
    paramAttrType_t paramType;
    class asynNDArrayDriver *pDriver;
};

#endif /*INCparamAttributeH*/
