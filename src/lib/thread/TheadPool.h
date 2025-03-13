#pragma once

#include <QObject>
#include <memory>
#include "base/basic_types.h"

class QThreadPool;

namespace lib::thread {

class ThreadPool : public QObject {
    Q_OBJECT
public:
    explicit ThreadPool(QObject* parent = nullptr);
    ~ThreadPool() override;

    std::unique_ptr<QThreadPool> create(size_t size);
    QThreadPool* global();
    void execute(ok::base::Fn<void()>&);

private:
     std::shared_ptr<QThreadPool> global_;
};

}  // namespace lib::thread
