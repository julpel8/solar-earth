import React from 'react';
import { Modal, ModalOverlay, Dialog, Button, Heading } from 'react-aria-components';

interface ConfirmModalProps {
    isOpen: boolean;
    onOpenChange: (isOpen: boolean) => void;
    onConfirm: () => void;
    title: string;
    description: string;
    confirmLabel?: string;
}

export const ConfirmModal: React.FC<ConfirmModalProps> = ({
    isOpen,
    onOpenChange,
    onConfirm,
    title,
    description,
    confirmLabel = 'Confirm',
}) => {
    return (
        <ModalOverlay
            isOpen={isOpen}
            onOpenChange={onOpenChange}
            className="halite-color-modal-overlay"
            isDismissable
        >
            <Modal className="halite-color-modal">
                <Dialog className="halite-color-dialog">
                    {({ close }) => (
                        <>
                            <div className="halite-color-modal-header">
                                <Heading slot="title">{title}</Heading>
                                <Button className="halite-color-modal-close" onPress={close}>
                                    ×
                                </Button>
                            </div>
                            <div className="halite-import-content">
                                <p style={{ margin: 0 }}>{description}</p>
                                <div style={{ display: 'flex', gap: '10px', justifyContent: 'flex-end', marginTop: '10px' }}>
                                    <Button className="halite-custom-action-btn" onPress={close}>
                                        Cancel
                                    </Button>
                                    <Button
                                        className="halite-custom-action-btn halite-custom-action-btn--danger"
                                        onPress={() => {
                                            onConfirm();
                                            close();
                                        }}
                                    >
                                        {confirmLabel}
                                    </Button>
                                </div>
                            </div>
                        </>
                    )}
                </Dialog>
            </Modal>
        </ModalOverlay>
    );
};
