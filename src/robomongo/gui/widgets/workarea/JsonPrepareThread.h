#pragma once

#include <QWidget>
#include <QThread>

#include "robomongo/core/Core.h"
#include "robomongo/core/domain/MongoQueryInfo.h"
#include "robomongo/core/domain/Enums.h"

namespace Robomongo
{
    class MongoShell;

    /*
    ** In this thread we are running task to prepare JSON string from list of BSON objects
    */
    class JsonPrepareThread : public QThread
    {
        Q_OBJECT

    public:
        /*
        ** Constructor
        */
        JsonPrepareThread(QList<MongoDocumentPtr> bsonObjects, UUIDEncoding uuidEncoding, SupportedTimes timeZone);
        void stop();
   Q_SIGNALS:
        /**
         * @brief Signals when all parts prepared
         */
        void done();

        /**
         * @brief Signals when json part is ready
         */
        void partReady(const QString &part);

    protected:

        /*
        ** Overload function
        */
        virtual void run();
    private:
        /*
        ** List of documents
        */
        const QList<MongoDocumentPtr> _bsonObjects;
        const UUIDEncoding _uuidEncoding;
        const SupportedTimes _timeZone;
        volatile bool _stop;
    };
}