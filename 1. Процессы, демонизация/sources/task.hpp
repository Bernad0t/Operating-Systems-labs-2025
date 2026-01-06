#pragma once

class Task {
public:
    virtual ~Task() = default;
    virtual void execute() = 0;
    virtual int interval() const = 0;
};