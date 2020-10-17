class ReliableSender;

class State {
    protected:
    ReliableSender *context_;

    public:
    State();
    State(ReliableSender *sender);
    virtual void dupACK() = 0;
    virtual void newACK(int ackId) = 0;
    void timeout();
    virtual ~State();
};

class FastRecovery : State {
    public:
    FastRecovery(ReliableSender *context);
    void dupACK();
    void newACK(int ackId);
};