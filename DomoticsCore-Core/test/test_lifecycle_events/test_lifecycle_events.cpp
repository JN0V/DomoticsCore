#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

class LifecycleTestComponent : public IComponent {
public:
    LifecycleTestComponent(const String& name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    
    ComponentStatus begin() override { 
        beginCalled = true;
        return ComponentStatus::Success; 
    }
    void loop() override { loopCalled = true; }
    ComponentStatus shutdown() override { 
        shutdownCalled = true;
        return ComponentStatus::Success; 
    }
    std::vector<Dependency> getDependencies() const override { return deps; }
    void addDependency(const String& name) { deps.push_back(Dependency{name, true}); }
    void afterAllComponentsReady() override { afterReadyCalled = true; }
    
    bool beginCalled = false;
    bool loopCalled = false;
    bool shutdownCalled = false;
    bool afterReadyCalled = false;
private:
    std::vector<Dependency> deps;
};

std::vector<String> shutdownOrder;

class ShutdownTracker : public IComponent {
public:
    ShutdownTracker(const String& name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override {}
    ComponentStatus shutdown() override { 
        shutdownOrder.push_back(metadata.name);
        return ComponentStatus::Success; 
    }
    std::vector<Dependency> getDependencies() const override { return deps; }
    void addDependency(const String& name) { deps.push_back(Dependency{name, true}); }
private:
    std::vector<Dependency> deps;
};

Core* testCore = nullptr;

void setUp(void) {
    testCore = new Core();
    shutdownOrder.clear();
}

void tearDown(void) {
    delete testCore;
    testCore = nullptr;
}

void test_begin_called_on_init(void) {
    auto comp = std::make_unique<LifecycleTestComponent>("TestComp");
    LifecycleTestComponent* ptr = comp.get();
    testCore->addComponent(std::move(comp));
    
    TEST_ASSERT_FALSE(ptr->beginCalled);
    testCore->begin();
    TEST_ASSERT_TRUE(ptr->beginCalled);
}

void test_loop_called(void) {
    auto comp = std::make_unique<LifecycleTestComponent>("TestComp");
    LifecycleTestComponent* ptr = comp.get();
    testCore->addComponent(std::move(comp));
    testCore->begin();
    
    TEST_ASSERT_FALSE(ptr->loopCalled);
    testCore->loop();
    TEST_ASSERT_TRUE(ptr->loopCalled);
}

void test_shutdown_called(void) {
    auto comp = std::make_unique<LifecycleTestComponent>("TestComp");
    LifecycleTestComponent* ptr = comp.get();
    testCore->addComponent(std::move(comp));
    testCore->begin();
    
    TEST_ASSERT_FALSE(ptr->shutdownCalled);
    testCore->shutdown();
    TEST_ASSERT_TRUE(ptr->shutdownCalled);
}

void test_shutdown_reverse_order(void) {
    auto compC = std::make_unique<ShutdownTracker>("C");
    compC->addDependency("B");
    auto compB = std::make_unique<ShutdownTracker>("B");
    compB->addDependency("A");
    auto compA = std::make_unique<ShutdownTracker>("A");
    
    testCore->addComponent(std::move(compC));
    testCore->addComponent(std::move(compB));
    testCore->addComponent(std::move(compA));
    testCore->begin();
    testCore->shutdown();
    
    TEST_ASSERT_EQUAL(3, shutdownOrder.size());
    TEST_ASSERT_EQUAL_STRING("C", shutdownOrder[0].c_str());
    TEST_ASSERT_EQUAL_STRING("B", shutdownOrder[1].c_str());
    TEST_ASSERT_EQUAL_STRING("A", shutdownOrder[2].c_str());
}

void test_after_all_components_ready(void) {
    auto comp = std::make_unique<LifecycleTestComponent>("TestComp");
    LifecycleTestComponent* ptr = comp.get();
    testCore->addComponent(std::move(comp));
    
    TEST_ASSERT_FALSE(ptr->afterReadyCalled);
    testCore->begin();
    TEST_ASSERT_TRUE(ptr->afterReadyCalled);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_begin_called_on_init);
    RUN_TEST(test_loop_called);
    RUN_TEST(test_shutdown_called);
    RUN_TEST(test_shutdown_reverse_order);
    RUN_TEST(test_after_all_components_ready);
    
    return UNITY_END();
}
