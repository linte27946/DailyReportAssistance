#pragma once

#include <QMap>
#include <QString>
#include <memory>
#include <functional>
#include <spdlog/spdlog.h>

/// Simple dependency injection container.
/// Services are registered by type name and can be resolved anywhere.
template <typename T>
class ServiceLocatorT {
public:
    using Factory = std::function<std::shared_ptr<T>()>;

    static ServiceLocatorT &instance()
    {
        static ServiceLocatorT<T> locator;
        return locator;
    }

    /// Register a factory function for the given name.
    void registerFactory(const QString &name, Factory factory)
    {
        m_factories[name] = std::move(factory);
    }

    /// Register a pre-built instance for the given name.
    void registerInstance(const QString &name, std::shared_ptr<T> instance)
    {
        m_instances[name] = instance;
        // Also register a factory that returns this instance
        registerFactory(name, [instance]() { return instance; });
    }

    /// Resolve a service by name. Returns the shared instance, creating it if needed.
    std::shared_ptr<T> resolve(const QString &name)
    {
        // Return existing instance if available
        if (m_instances.contains(name)) {
            return m_instances[name];
        }

        // Create via factory
        if (m_factories.contains(name)) {
            auto instance = m_factories[name]();
            m_instances[name] = instance;
            return instance;
        }

        spdlog::warn("Service not found: {}", name.toStdString());
        return nullptr;
    }

    /// Check if a service is registered.
    bool has(const QString &name) const
    {
        return m_factories.contains(name) || m_instances.contains(name);
    }

    /// Remove all registrations.
    void clear()
    {
        m_factories.clear();
        m_instances.clear();
    }

private:
    ServiceLocatorT() = default;
    QMap<QString, Factory> m_factories;
    QMap<QString, std::shared_ptr<T>> m_instances;
};

/// Convenience type alias for common service locator usage.
using ServiceLocator = ServiceLocatorT<void>;
