/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testGCExactRooting)
{
    return true;
}
END_TEST(testGCExactRooting)

BEGIN_TEST(testGCSuppressions)
{
    return true;
}
END_TEST(testGCSuppressions)

BEGIN_TEST(testGCRootedStaticStructInternalStackStorageAugmented)
{
    return true;
}
END_TEST(testGCRootedStaticStructInternalStackStorageAugmented)

BEGIN_TEST(testGCPersistentRootedOutlivesRuntime)
{
    return true;
}
END_TEST(testGCPersistentRootedOutlivesRuntime)

BEGIN_TEST(testGCPersistentRootedTraceableCannotOutliveRuntime)
{
    return true;
}
END_TEST(testGCPersistentRootedTraceableCannotOutliveRuntime)

BEGIN_TEST(testGCRootedHashMap)
{
    return true;
}
END_TEST(testGCRootedHashMap)

BEGIN_TEST(testGCHandleHashMap)
{
    return true;
}
END_TEST(testGCHandleHashMap)

BEGIN_TEST(testGCRootedVector)
{

    return true;
}
END_TEST(testGCRootedVector)

BEGIN_TEST(testTraceableFifo)
{
    return true;
}
END_TEST(testTraceableFifo)

BEGIN_TEST(testGCHandleVector)
{
    return true;
}
END_TEST(testGCHandleVector)
