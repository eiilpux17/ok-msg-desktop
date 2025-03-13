#pragma once

#include "TheadPool.h"

#include <QThreadPool>


namespace lib::thread {

ThreadPool::ThreadPool(QObject* parent)
        : QObject(parent)
        , global_(std::shared_ptr<QThreadPool>(QThreadPool::globalInstance())){

}

std::unique_ptr<QThreadPool> ThreadPool::create(size_t size)
{
    assert(size > 0);

    auto customPool = std::make_unique<QThreadPool>(this);
    // customPool->setMaxThreadCount(2);
    // customPool->start(task);
    return customPool;
}

QThreadPool* ThreadPool::global()
{
    return global_.get();
}

void ThreadPool::execute(ok::base::Fn<void ()>& fn)
{
    global_->start(fn);
}

}  // namespace lib::thread
