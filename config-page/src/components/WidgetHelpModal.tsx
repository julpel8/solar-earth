import React from 'react';
import { Button, Dialog, Heading, Modal, ModalOverlay } from 'react-aria-components';
import { WidgetToken } from '../data/widgetTypes';
import { renderPreview } from '../data/i18nPreview';

interface WidgetHelpModalProps {
    isOpen: boolean;
    tokens: WidgetToken[];
    lang: number;
    isImperial: boolean;
    altLabel: string;
    altLabel2: string;
    onOpenChange: (isOpen: boolean) => void;
}

const compareLabels = (a: string, b: string) =>
    a.localeCompare(b, undefined, { sensitivity: 'base' });

const groupTokens = (tokens: WidgetToken[]) => {
    const groups: Record<string, WidgetToken[]> = {};
    tokens.forEach((token) => {
        if (!groups[token.category]) groups[token.category] = [];
        groups[token.category].push(token);
    });
    return groups;
};

export const WidgetHelpModal: React.FC<WidgetHelpModalProps> = ({
    isOpen,
    tokens,
    lang,
    isImperial,
    altLabel,
    altLabel2,
    onOpenChange,
}) => {
    const tokenGroups = React.useMemo(() => groupTokens(tokens), [tokens]);

    return (
        <ModalOverlay
            isOpen={isOpen}
            onOpenChange={onOpenChange}
            className="halite-color-modal-overlay"
            isDismissable
        >
            <Modal className="halite-color-modal widget-help-modal">
                <Dialog className="halite-color-dialog">
                    {({ close }) => (
                        <>
                            <div className="halite-color-modal-header">
                                <Heading slot="title">Text variables</Heading>
                                <Button className="halite-color-modal-close" onPress={close}>
                                    ×
                                </Button>
                            </div>
                            <div className="widget-help-content">
                                <p className="widget-help-intro">
                                    Mix plain text with variables wrapped in braces. Each variable is
                                    replaced with live data from your watch. Example values use your
                                    current language and units.
                                </p>
                                {Object.entries(tokenGroups)
                                    .sort(([a], [b]) => compareLabels(a, b))
                                    .map(([category, groupItems]) => (
                                        <div className="widget-help-group" key={category}>
                                            <div className="widget-token-group-label">{category}</div>
                                            <ul className="widget-help-list">
                                                {groupItems.map((token) => {
                                                    const example = renderPreview(
                                                        token.token,
                                                        lang,
                                                        isImperial,
                                                        altLabel,
                                                        altLabel2,
                                                    );
                                                    return (
                                                        <li className="widget-help-item" key={token.token}>
                                                            <div className="widget-help-item-head">
                                                                <code className="widget-help-token">{token.token}</code>
                                                                {example && (
                                                                    <span className="widget-help-example">{example}</span>
                                                                )}
                                                            </div>
                                                            {token.description && (
                                                                <span className="widget-help-desc">{token.description}</span>
                                                            )}
                                                        </li>
                                                    );
                                                })}
                                            </ul>
                                        </div>
                                    ))}
                            </div>
                        </>
                    )}
                </Dialog>
            </Modal>
        </ModalOverlay>
    );
};
