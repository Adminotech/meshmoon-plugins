#import <Sparkle/SUUpdater.h>
#import "SparkleAutoUpdater.h"

@interface UpdaterDelegate : NSObject
{
    SparkleAutoUpdater *cppUpdater;
}

-(void)setCPPUpdater:(SparkleAutoUpdater*)u;
@end
